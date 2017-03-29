#include "stream_transforming_async_channel.h"

namespace nx {
namespace network {
namespace aio {

StreamTransformingAsyncChannel::StreamTransformingAsyncChannel(
    std::unique_ptr<AbstractAsyncChannel> rawDataChannel,
    std::unique_ptr<nx::utils::pipeline::Converter> converter)
    :
    m_rawDataChannel(std::move(rawDataChannel)),
    m_converter(std::move(converter)),
    m_userReadBuffer(nullptr),
    m_bytesEncodedOnPreviousStep(0),
    m_asyncReadInProgress(false)
{
    using namespace std::placeholders;

    m_inputPipeline = utils::pipeline::makeCustomInputPipeline(
        std::bind(&StreamTransformingAsyncChannel::readRawBytes, this, _1, _2));
    m_outputPipeline = utils::pipeline::makeCustomOutputPipeline(
        std::bind(&StreamTransformingAsyncChannel::writeRawBytes, this, _1, _2));
    m_converter->setInput(m_inputPipeline.get());
    m_converter->setOutput(m_outputPipeline.get());

    bindToAioThread(getAioThread());
}

StreamTransformingAsyncChannel::~StreamTransformingAsyncChannel()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void StreamTransformingAsyncChannel::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_rawDataChannel->bindToAioThread(aioThread);
}

void StreamTransformingAsyncChannel::readSomeAsync(
    nx::Buffer* const buffer,
    UserIoHandler handler)
{
    using namespace std::placeholders;

    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            m_userTaskQueue.push_back(
                std::make_unique<ReadTask>(buffer, std::move(handler)));
            tryToCompleteNextUserTask();
        });
}

void StreamTransformingAsyncChannel::sendAsync(
    const nx::Buffer& buffer,
    UserIoHandler handler)
{
    using namespace std::placeholders;

    post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            m_userTaskQueue.push_back(
                std::make_unique<WriteTask>(buffer, std::move(handler)));
            tryToCompleteNextUserTask();
        });
}

void StreamTransformingAsyncChannel::cancelIOSync(
    nx::network::aio::EventType eventType)
{
    m_rawDataChannel->cancelIOSync(eventType);
}

void StreamTransformingAsyncChannel::stopWhileInAioThread()
{
    m_rawDataChannel.reset();
}

int StreamTransformingAsyncChannel::readRawBytes(void* data, size_t count)
{
    using namespace std::placeholders;

    NX_ASSERT(isInSelfAioThread());

    if (!m_readRawData.empty())
        return readRawDataFromCache(data, count);

    if (!m_asyncReadInProgress)
        readRawChannelAsync();

    return utils::pipeline::StreamIoError::wouldBlock;
}

int StreamTransformingAsyncChannel::writeRawBytes(const void* data, size_t count)
{
    using namespace std::placeholders;

    NX_ASSERT(isInSelfAioThread());

    // TODO: #ak Put a limit on send queue size.

    m_rawWriteQueue.push_back(nx::Buffer((const char*)data, (int)count));
    if (m_rawWriteQueue.size() == 1)
    {
        m_rawDataChannel->sendAsync(
            m_rawWriteQueue.front(),
            std::bind(&StreamTransformingAsyncChannel::onRawDataWritten, this, _1, _2));
    }

    return (int)count;
}

int StreamTransformingAsyncChannel::readRawDataFromCache(void* data, size_t count)
{
    uint8_t* dataBytes = static_cast<uint8_t*>(data);
    std::size_t bytesRead = 0;

    while (!m_readRawData.empty() && count > 0)
    {
        nx::Buffer& rawData = m_readRawData.front();
        auto bytesToCopy = std::min<std::size_t>(rawData.size(), count);
        memcpy(dataBytes, rawData.constData(), bytesToCopy);
        dataBytes += bytesToCopy;
        count -= bytesToCopy;
        rawData.remove(0, (int)bytesToCopy);
        if (m_readRawData.front().isEmpty())
            m_readRawData.pop_front();
        bytesRead += bytesToCopy;
    }

    return (int)bytesRead;
}

void StreamTransformingAsyncChannel::readRawChannelAsync()
{
    using namespace std::placeholders;

    constexpr static std::size_t kRawReadBufferSize = 16 * 1024;

    m_rawDataReadBuffer.clear();
    m_rawDataReadBuffer.reserve(kRawReadBufferSize);
    m_rawDataChannel->readSomeAsync(
        &m_rawDataReadBuffer,
        std::bind(&StreamTransformingAsyncChannel::onSomeRawDataRead, this, _1, _2));
    m_asyncReadInProgress = true;
}

void StreamTransformingAsyncChannel::tryToCompleteNextUserTask()
{
    //while (!m_userTaskQueue.empty())
    {
        UserTask* task = m_userTaskQueue.front().get();

        utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
        processTask(task);
        if (watcher.objectDestroyed())
            return;

        NX_ASSERT(m_userTaskQueue.front().get() == task);
        if (m_userTaskQueue.front()->status == UserTaskStatus::done)
            m_userTaskQueue.pop_front();
    }
}

void StreamTransformingAsyncChannel::processTask(UserTask* task)
{
    switch (task->type)
    {
        case UserTaskType::read:
            processReadTask(static_cast<ReadTask*>(task));
            break;

        case UserTaskType::write:
            processWriteTask(static_cast<WriteTask*>(task));
            break;

        default:
            NX_CRITICAL(false);
    }
}

void StreamTransformingAsyncChannel::processReadTask(ReadTask* task)
{
    processUserTask(
        std::bind(&utils::pipeline::Converter::read, m_converter.get(),
            task->buffer->data() + task->buffer->size(),
            task->buffer->capacity() - task->buffer->size()),
        task);
}

void StreamTransformingAsyncChannel::processWriteTask(WriteTask* task)
{
    processUserTask(
        std::bind(&utils::pipeline::Converter::write, m_converter.get(),
            task->buffer.data(),
            task->buffer.size()),
        task);
}

template<typename TransformerFunc>
void StreamTransformingAsyncChannel::processUserTask(
    TransformerFunc func,
    UserTask* task)
{
    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    int bytesTransferred = 0;
    std::tie(sysErrorCode, bytesTransferred) = invokeTransformer(std::move(func));
    if (sysErrorCode != SystemError::wouldBlock) //< Operation finished?
    {
        auto userHandler = std::move(task->handler);
        task->status = UserTaskStatus::done;
        userHandler(sysErrorCode, bytesTransferred);
    }
}

template<typename TransformerFunc>
std::tuple<SystemError::ErrorCode, int /*bytesTransferred*/>
    StreamTransformingAsyncChannel::invokeTransformer(
        TransformerFunc func)
{
    const int result = func();
    if (result > 0)
        return std::make_tuple(SystemError::noError, result);

    if (m_converter->failed())
    {
        // This is actually converter error. E.g., ssl stream corruption. Not a system error!
        // TODO: #ak Set converter-specific error code.
        return std::make_tuple(SystemError::connectionReset, -1);
    }

    if (m_converter->eof())
    {
        // Correct stream shutdown.
        return std::make_tuple(SystemError::noError, 0);
    }

    NX_ASSERT(result == utils::pipeline::StreamIoError::wouldBlock);
    return std::make_tuple(SystemError::wouldBlock, -1);
}

void StreamTransformingAsyncChannel::onSomeRawDataRead(
    SystemError::ErrorCode sysErrorCode,
    std::size_t bytesTransferred)
{
    m_asyncReadInProgress = false;

    if (sysErrorCode == SystemError::noError && bytesTransferred > 0)
    {
        decltype(m_rawDataReadBuffer) buf;
        m_rawDataReadBuffer.swap(buf);
        m_readRawData.push_back(std::move(buf));
        tryToCompleteNextUserTask();
        return;
    }

    handleIoError(sysErrorCode);
}

void StreamTransformingAsyncChannel::onRawDataWritten(
    SystemError::ErrorCode sysErrorCode,
    std::size_t /*bytesTransferred*/)
{
    using namespace std::placeholders;

    m_rawWriteQueue.pop_front();
    if (!m_rawWriteQueue.empty())
    {
        m_rawDataChannel->sendAsync(
            m_rawWriteQueue.front(),
            std::bind(&StreamTransformingAsyncChannel::onRawDataWritten, this, _1, _2));
    }

    if (sysErrorCode == SystemError::noError)
        tryToCompleteNextUserTask();

    handleIoError(sysErrorCode);
}

void StreamTransformingAsyncChannel::handleIoError(SystemError::ErrorCode sysErrorCode)
{
    // We can have SystemError::noError here in case of a connection closure.
    decltype(m_userTaskQueue) userTaskQueue;
    userTaskQueue.swap(m_userTaskQueue);
    for (auto& userTask: userTaskQueue)
    {
        auto handler = std::move(userTask->handler);
        utils::ObjectDestructionFlag::Watcher thisDestructionWatcher(&m_destructionFlag);
        if (sysErrorCode == SystemError::noError) //< Connection closed.
        {
            if (userTask->type == UserTaskType::read)
                handler(sysErrorCode, 0);
            else
                handler(SystemError::connectionReset, (std::size_t)-1);
        }
        else
        {
            handler(sysErrorCode, (std::size_t)-1);
        }
        if (thisDestructionWatcher.objectDestroyed())
            return;
    }
}

} // namespace aio
} // namespace network
} // namespace nx
