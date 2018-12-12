#include "stream_transforming_async_channel.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>

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
    m_inputPipeline = utils::bstream::makeCustomInput(
        [this](auto&&... args) { return readRawBytes(std::move(args)...); });
    m_outputPipeline = utils::bstream::makeCustomOutput(
        [this](auto&&... args) { return writeRawBytes(std::move(args)...); });
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
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            m_userTaskQueue.push_back(
                std::make_shared<ReadTask>(buffer, std::move(handler)));
            tryToCompleteUserTasks();
        });
}

void StreamTransformingAsyncChannel::sendAsync(
    const nx::Buffer& buffer,
    UserIoHandler handler)
{
    post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            m_userTaskQueue.push_back(
                std::make_shared<WriteTask>(buffer, std::move(handler)));
            tryToCompleteUserTasks();
        });
}

void StreamTransformingAsyncChannel::stopWhileInAioThread()
{
    m_rawDataChannel.reset();
}

void StreamTransformingAsyncChannel::tryToCompleteUserTasks()
{
    const auto tasksToProcess = m_userTaskQueue;
    // m_userTaskQueue can be changed during processing.
    tryToCompleteUserTasks(tasksToProcess);
}

void StreamTransformingAsyncChannel::tryToCompleteUserTasks(
    const std::deque<std::shared_ptr<UserTask>>& tasksToProcess)
{
    for (const std::shared_ptr<UserTask>& task: tasksToProcess)
    {
        InterruptionFlag::ScopeWatcher watcher(this, &m_aioInterruptionFlag);
        processTask(task.get());
        if (watcher.interrupted())
            return;

        if (task->status == UserTaskStatus::done)
            removeUserTask(task.get());
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
    NX_VERBOSE(this, lm("Processing read task. Read buffer size %1")
        .args(task->buffer->capacity() - task->buffer->size()));

    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    int bytesRead = 0;
    std::tie(sysErrorCode, bytesRead) = invokeConverter(
        std::bind(&utils::bstream::Converter::read, m_converter,
            task->buffer->data() + task->buffer->size(),
            task->buffer->capacity() - task->buffer->size()));
    if (sysErrorCode == SystemError::wouldBlock)
    {
        NX_VERBOSE(this, "Failed to process read task. wouldBlock");
        return;
    }

    NX_VERBOSE(this, lm("Read task completed. Result %1, bytesRead %2")
        .args(sysErrorCode, bytesRead));

    if (sysErrorCode == SystemError::noError && bytesRead > 0)
        task->buffer->resize(task->buffer->size() + bytesRead);

    task->status = UserTaskStatus::done;
    auto userHandler = std::move(task->handler);
    userHandler(sysErrorCode, bytesRead);
}

void StreamTransformingAsyncChannel::processWriteTask(WriteTask* task)
{
    NX_VERBOSE(this, lm("Processing write task (%1 bytes)").args(task->buffer.size()));

    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    int bytesWritten = 0;

    const auto rawWriteQueueSizeBak = m_rawWriteQueue.size();

    std::tie(sysErrorCode, bytesWritten) = invokeConverter(
        std::bind(&utils::bstream::Converter::write, m_converter,
            task->buffer.data(),
            task->buffer.size()));
    if (sysErrorCode == SystemError::wouldBlock)
    {
        NX_VERBOSE(this, "Failed to process write task. wouldBlock");
        return; //< Could not schedule user data send.
    }

    task->status = UserTaskStatus::done;

    NX_VERBOSE(this, lm("Write task completed. Result %1, bytesWritten %2")
        .args(sysErrorCode, bytesWritten));

    if (m_rawWriteQueue.size() > rawWriteQueueSizeBak)
    {
        // User data send has been scheduled.
        // Reporting result only after send completion on underlying raw channel.
        NX_ASSERT(!m_rawWriteQueue.empty());
        m_rawWriteQueue.back().userHandler =
            std::exchange(task->handler, nullptr);
        m_rawWriteQueue.back().userByteCount = task->buffer.size();
    }
    else
    {
        nx::utils::swapAndCall(task->handler, sysErrorCode, 0);
    }
}

template<typename TransformerFunc>
std::tuple<SystemError::ErrorCode, int /*bytesTransferred*/>
    StreamTransformingAsyncChannel::invokeConverter(
        TransformerFunc func)
{
    const int result = func();
    if (result >= 0)
        return std::make_tuple(SystemError::noError, result);

    if (m_converter->failed())
    {
        NX_VERBOSE(this, "Converter reported failure");
        // This is actually converter error. E.g., ssl stream corruption. Not a system error!
        // TODO: #ak Set converter-specific error code.
        return std::make_tuple(SystemError::connectionReset, -1);
    }

    if (m_converter->eof())
    {
        NX_VERBOSE(this, "Converter reported EOF");
        // Correct stream shutdown.
        return std::make_tuple(SystemError::noError, 0);
    }

    // Converter has not moved to "failed" state, so we have a recoverable error here.

    NX_ASSERT(
        result == utils::bstream::StreamIoError::wouldBlock ||
            result == utils::bstream::StreamIoError::osError,
        lm("result = %1").args(result));
    return std::make_tuple(SystemError::wouldBlock, -1);
}

int StreamTransformingAsyncChannel::readRawBytes(void* data, size_t count)
{
    NX_ASSERT(isInSelfAioThread());

    if (!m_readRawData.empty())
        return readRawDataFromCache(data, count);

    if (!m_asyncReadInProgress)
        readRawChannelAsync();

    return utils::bstream::StreamIoError::wouldBlock;
}

int StreamTransformingAsyncChannel::readRawDataFromCache(
    void* data,
    const size_t count)
{
    uint8_t* dataBytes = static_cast<uint8_t*>(data);
    std::size_t bytesRead = 0;

    auto bytesToReadCount = count;
    while (!m_readRawData.empty() && bytesToReadCount > 0)
    {
        nx::Buffer& rawData = m_readRawData.front();
        auto bytesToCopy = std::min<std::size_t>(rawData.size(), bytesToReadCount);
        memcpy(dataBytes, rawData.constData(), bytesToCopy);
        dataBytes += bytesToCopy;
        bytesToReadCount -= bytesToCopy;
        rawData.remove(0, (int)bytesToCopy);
        if (m_readRawData.front().isEmpty())
            m_readRawData.pop_front();
        bytesRead += bytesToCopy;
    }

    NX_VERBOSE(this, lm("%1 bytes read from cache. %2 bytes were requested")
        .args(bytesRead, count));

    return (int)bytesRead;
}

void StreamTransformingAsyncChannel::readRawChannelAsync()
{
    constexpr static std::size_t kRawReadBufferSize = 16 * 1024;

    NX_VERBOSE(this, lm("Scheduling socket read operation"));

    m_rawDataReadBuffer.clear();
    m_rawDataReadBuffer.reserve(kRawReadBufferSize);
    m_rawDataChannel->readSomeAsync(
        &m_rawDataReadBuffer,
        [this](auto&&... args) { onSomeRawDataRead(std::move(args)...); });
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

    if (nx::network::socketCannotRecoverFromError(sysErrorCode))
        return reportFailureOfEveryUserTask(sysErrorCode);

    // Reporting failure to user task(s) that depend on this read.
    reportFailureToTasksFilteredByType(sysErrorCode, UserTaskType::read);
}

int StreamTransformingAsyncChannel::writeRawBytes(const void* data, size_t count)
{
    NX_ASSERT(isInSelfAioThread());

    // TODO: #ak Put a limit on send queue size.

    m_rawWriteQueue.push_back(RawSendContext());
    m_rawWriteQueue.back().data = nx::Buffer((const char*)data, (int)count);
    if (m_rawWriteQueue.size() == 1)
    {
        if (m_sendShutdown)
            post([this]() { onRawDataWritten(SystemError::connectionReset, (size_t) -1); });
        else
            scheduleNextRawSendTaskIfAny();
    }

    return (int)count;
}

void StreamTransformingAsyncChannel::onRawDataWritten(
    SystemError::ErrorCode resultCode,
    std::size_t /*bytesTransferred*/)
{
    auto completedIoRange =
        std::make_pair(m_rawWriteQueue.begin(), std::next(m_rawWriteQueue.begin()));
    if (resultCode != SystemError::noError)
    {
        // Marking socket unusable since we cannot throw away bytes out of SSL stream.
        m_sendShutdown = true;

        // Considering every queue raw write failed since send has been shut down.
        completedIoRange.second = m_rawWriteQueue.end();
    }

    if (completeRawSendTasks(takeRawSendTasks(completedIoRange), resultCode) !=
        InterruptionFlag::StateChange::noChange)
    {
        return;
    }

    if (resultCode == SystemError::noError)
    {
        scheduleNextRawSendTaskIfAny();

        // NOTE: Not trying to complete user tasks added in send user handler 
        // (in completeRawSendTasks) because aio thread could have been changed by user.
        // So, we will complete user tasks in a proper aio thread on the next event.
        tryToCompleteUserTasks();
        return;
    }

    if (socketCannotRecoverFromError(resultCode))
        reportFailureOfEveryUserTask(resultCode);
    else
        reportFailureToTasksFilteredByType(resultCode, UserTaskType::write);
}

template<typename Range>
std::deque<StreamTransformingAsyncChannel::RawSendContext> 
    StreamTransformingAsyncChannel::takeRawSendTasks(Range range)
{
    decltype(m_rawWriteQueue) rawSendTasks;
    std::move(
        range.first, range.second,
        std::back_inserter(rawSendTasks));
    m_rawWriteQueue.erase(range.first, range.second);

    return rawSendTasks;
}

InterruptionFlag::StateChange StreamTransformingAsyncChannel::completeRawSendTasks(
    std::deque<RawSendContext> completedRawSendTasks,
    SystemError::ErrorCode sysErrorCode)
{
    for (auto& sendTask: completedRawSendTasks)
    {
        if (!sendTask.userHandler)
            continue;

        InterruptionFlag::ScopeWatcher interruptionWatcher(this, &m_aioInterruptionFlag);
        nx::utils::swapAndCall(
            sendTask.userHandler,
            sysErrorCode,
            sysErrorCode == SystemError::noError ? sendTask.userByteCount : (size_t) -1);
        if (interruptionWatcher.interrupted())
            return interruptionWatcher.stateChange();
    }

    return InterruptionFlag::StateChange::noChange;
}

void StreamTransformingAsyncChannel::scheduleNextRawSendTaskIfAny()
{
    if (!m_rawWriteQueue.empty())
    {
        NX_VERBOSE(this, lm("Scheduling socket write operation"));

        m_rawDataChannel->sendAsync(
            m_rawWriteQueue.front().data,
            [this](auto&&... args) { onRawDataWritten(std::move(args)...); });
    }
}

void StreamTransformingAsyncChannel::reportFailureOfEveryUserTask(
    SystemError::ErrorCode sysErrorCode)
{
    reportFailureToTasksFilteredByType(sysErrorCode, std::nullopt);
}

void StreamTransformingAsyncChannel::reportFailureToTasksFilteredByType(
    SystemError::ErrorCode sysErrorCode,
    std::optional<UserTaskType> userTypeFilter)
{
    // We can have SystemError::noError here in case of a connection closure.
    decltype(m_userTaskQueue) userTaskQueue;
    if (!userTypeFilter)
    {
        userTaskQueue.swap(m_userTaskQueue);
    }
    else
    {
        auto movedTaskRangeIter = nx::utils::move_if(
            m_userTaskQueue.begin(), m_userTaskQueue.end(),
            std::back_inserter(userTaskQueue),
            [&userTypeFilter](const auto& userTask) { return userTask->type == *userTypeFilter; });
        m_userTaskQueue.erase(movedTaskRangeIter, m_userTaskQueue.end());
    }

    for (auto& userTask: userTaskQueue)
    {
        auto handler = std::move(userTask->handler);
        InterruptionFlag::ScopeWatcher interruptionWatcher(this, &m_aioInterruptionFlag);
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
        if (interruptionWatcher.interrupted())
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

void StreamTransformingAsyncChannel::cancelIoInAioThread(aio::EventType eventType)
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

    // Needed for pleaseStop to work correctly.
    // Should be removed when AbstractStreamSocket inherits BasicPollable.
    if (eventType == aio::EventType::etNone)
        m_rawDataChannel->cancelIOSync(aio::EventType::etNone);
}

} // namespace aio
} // namespace network
} // namespace nx
