#include "stream_transforming_async_channel.h"

namespace nx {
namespace network {
namespace aio {

StreamTransformingAsyncChannel::StreamTransformingAsyncChannel(
    std::unique_ptr<AbstractAsyncChannel> rawDataChannel,
    nx::utils::bstream::Converter* converter)
    :
    m_rawDataChannel(std::move(rawDataChannel)),
    m_converter(converter),
    m_asyncReadInProgress(false)
{
    using namespace std::placeholders;

    m_inputPipeline = utils::bstream::makeCustomInput(
        std::bind(&StreamTransformingAsyncChannel::readRawBytes, this, _1, _2));
    m_outputPipeline = utils::bstream::makeCustomOutput(
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
            tryToCompleteUserTasks();
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
            tryToCompleteUserTasks();
        });
}

void StreamTransformingAsyncChannel::cancelIOSync(
    aio::EventType eventType)
{
    executeInAioThreadSync(std::bind(
        &StreamTransformingAsyncChannel::cancelIoWhileInAioThread, this, eventType));
}

void StreamTransformingAsyncChannel::stopWhileInAioThread()
{
    m_rawDataChannel.reset();
}

void StreamTransformingAsyncChannel::tryToCompleteUserTasks()
{
    std::vector<UserTask*> tasksToProcess;
    tasksToProcess.reserve(m_userTaskQueue.size());
    for (auto& task: m_userTaskQueue)
        tasksToProcess.push_back(task.get());

    // m_userTaskQueue can be changed during processing.

    for (UserTask* task: tasksToProcess)
    {
        utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
        processTask(task);
        if (watcher.objectDestroyed())
            return;

        if (task->status == UserTaskStatus::done)
            removeUserTask(task);
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
    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    int bytesRead = 0;
    std::tie(sysErrorCode, bytesRead) = invokeConverter(
        std::bind(&utils::bstream::Converter::read, m_converter,
            task->buffer->data() + task->buffer->size(),
            task->buffer->capacity() - task->buffer->size()));
    if (sysErrorCode == SystemError::wouldBlock)
        return;

    task->buffer->resize(task->buffer->size() + bytesRead);

    task->status = UserTaskStatus::done;
    auto userHandler = std::move(task->handler);
    userHandler(sysErrorCode, bytesRead);
}

void StreamTransformingAsyncChannel::processWriteTask(WriteTask* task)
{
    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    int bytesWritten = 0;
    std::tie(sysErrorCode, bytesWritten) = invokeConverter(
        std::bind(&utils::bstream::Converter::write, m_converter,
            task->buffer.data(),
            task->buffer.size()));
    if (sysErrorCode == SystemError::wouldBlock)
        return;

    task->status = UserTaskStatus::done;
    auto userHandler = std::move(task->handler);
    userHandler(sysErrorCode, bytesWritten);
}

template<typename TransformerFunc>
std::tuple<SystemError::ErrorCode, int /*bytesTransferred*/>
    StreamTransformingAsyncChannel::invokeConverter(
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

    NX_ASSERT(result == utils::bstream::StreamIoError::wouldBlock);
    return std::make_tuple(SystemError::wouldBlock, -1);
}

int StreamTransformingAsyncChannel::readRawBytes(void* data, size_t count)
{
    using namespace std::placeholders;

    NX_ASSERT(isInSelfAioThread());

    if (!m_readRawData.empty())
        return readRawDataFromCache(data, count);

    if (!m_asyncReadInProgress)
        readRawChannelAsync();

    return utils::bstream::StreamIoError::wouldBlock;
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
        tryToCompleteUserTasks();
        return;
    }

    handleIoError(sysErrorCode);
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

void StreamTransformingAsyncChannel::onRawDataWritten(
    SystemError::ErrorCode sysErrorCode,
    std::size_t bytesTransferred)
{
    using namespace std::placeholders;

    if (sysErrorCode == SystemError::noError)
    {
        NX_ASSERT(bytesTransferred == static_cast<std::size_t>(m_rawWriteQueue.front().size()));
        m_rawWriteQueue.pop_front();
        if (!m_rawWriteQueue.empty())
        {
            m_rawDataChannel->sendAsync(
                m_rawWriteQueue.front(),
                std::bind(&StreamTransformingAsyncChannel::onRawDataWritten, this, _1, _2));
        }

        return tryToCompleteUserTasks();
    }

    if (nx::network::socketCannotRecoverFromError(sysErrorCode))
        return handleIoError(sysErrorCode);

    // Retrying I/O.
    m_rawDataChannel->sendAsync(
        m_rawWriteQueue.front(),
        std::bind(&StreamTransformingAsyncChannel::onRawDataWritten, this, _1, _2));
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

void StreamTransformingAsyncChannel::removeUserTask(UserTask* taskToRemove)
{
    for (auto it = m_userTaskQueue.begin(); it != m_userTaskQueue.end(); ++it)
    {
        if (it->get() == taskToRemove)
        {
            m_userTaskQueue.erase(it);
            break;
        }
    }
}

void StreamTransformingAsyncChannel::cancelIoWhileInAioThread(aio::EventType eventType)
{
    // Removing user task and cancelling operations on underlying
    //   raw channel that are required by the task cancelled.

    for (auto it = m_userTaskQueue.begin(); it != m_userTaskQueue.end();)
    {
        if (it->get()->type == UserTaskType::read &&
            (eventType == aio::EventType::etRead ||
                eventType == aio::EventType::etNone))
        {
            it = m_userTaskQueue.erase(it);
            if (m_asyncReadInProgress)
            {
                m_rawDataChannel->cancelIOSync(aio::EventType::etRead);
                m_asyncReadInProgress = false;
            }
        }
        else if (it->get()->type == UserTaskType::write &&
            (eventType == aio::EventType::etWrite ||
                eventType == aio::EventType::etNone))
        {
            it = m_userTaskQueue.erase(it);
            m_rawDataChannel->cancelIOSync(aio::EventType::etWrite);
            m_rawWriteQueue.clear();
        }
        else
        {
            ++it;
        }
    }
}

} // namespace aio
} // namespace network
} // namespace nx
