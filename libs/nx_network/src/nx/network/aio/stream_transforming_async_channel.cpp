// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_transforming_async_channel.h"

#include <sstream>

#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>

namespace nx::network::aio {

StreamTransformingAsyncChannel::StreamTransformingAsyncChannel(
    std::unique_ptr<AbstractAsyncChannel> rawDataChannel,
    nx::utils::bstream::Converter* converter)
    :
    m_rawDataChannel(std::move(rawDataChannel)),
    m_converter(converter)
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
    const auto aioThreadBak = getAioThread();

    if (getAioThread() != aioThread)
        m_aioInterruptionFlag.interrupt();

    base_type::bindToAioThread(aioThread);

    m_readScheduler.bindToAioThread(aioThread);
    m_sendScheduler.bindToAioThread(aioThread);
    m_rawDataChannel->bindToAioThread(aioThread);

    NX_CRITICAL(aioThreadBak == aioThread || m_userTaskQueue.empty(),
        toString(m_userTaskQueue));
}

void StreamTransformingAsyncChannel::readSomeAsync(
    nx::Buffer* const buffer,
    UserIoHandler handler)
{
    m_readScheduler.post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            m_userTaskQueue.push_back(
                std::make_shared<ReadTask>(buffer, std::move(handler)));
            tryToCompleteUserTasks();
        });
}

void StreamTransformingAsyncChannel::sendAsync(
    const nx::Buffer* buffer,
    UserIoHandler handler)
{
    m_sendScheduler.post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            m_userTaskQueue.push_back(
                std::make_shared<WriteTask>(buffer, std::move(handler)));
            tryToCompleteUserTasks();
        });
}

void StreamTransformingAsyncChannel::stopWhileInAioThread()
{
    m_readScheduler.pleaseStopSync();
    m_sendScheduler.pleaseStopSync();
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
    if (m_pauseLevel > 0)
        return;

    for (const std::shared_ptr<UserTask>& task: tasksToProcess)
    {
        nx::utils::InterruptionFlag::Watcher watcher(&m_aioInterruptionFlag);

        try
        {
            processTask(task.get());
        }
        catch (const std::exception& e)
        {
            // NOTE: this may be already deleted here: it is tested by watcher.interrupted() below.
            NX_DEBUG(typeid(StreamTransformingAsyncChannel), "%1. Exception caught while reporting "
                "SSL I/O completion. %2", (void*) this, e.what());
        }

        if (watcher.interrupted())
            return;

        if (task->status == detail::UserTaskStatus::done)
            removeUserTask(task.get());
    }
}

void StreamTransformingAsyncChannel::processTask(UserTask* task)
{
    NX_CRITICAL(task->status != detail::UserTaskStatus::done, toString(*task));

    switch (task->type)
    {
        case detail::UserTaskType::read:
            processReadTask(static_cast<ReadTask*>(task));
            break;

        case detail::UserTaskType::write:
            processWriteTask(static_cast<WriteTask*>(task));
            break;

        default:
            NX_CRITICAL(false);
    }
}

void StreamTransformingAsyncChannel::processReadTask(ReadTask* task)
{
    NX_TRACE(this, "Processing read task. Read buffer size %1",
        task->buffer->capacity() - task->buffer->size());
    NX_ASSERT(isInSelfAioThread());

    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    int bytesRead = 0;

    const auto bufferSizeBak = task->buffer->size();
    task->buffer->resize(task->buffer->capacity());
    std::tie(sysErrorCode, bytesRead) = invokeConverter(
        std::bind(&utils::bstream::Converter::read, m_converter,
            task->buffer->data() + bufferSizeBak,
            task->buffer->capacity() - bufferSizeBak));
    if (sysErrorCode == SystemError::wouldBlock)
    {
        task->buffer->resize(bufferSizeBak);
        NX_TRACE(this, "Failed to process read task. wouldBlock");
        return;
    }

    NX_TRACE(this, "Read task completed. Result %1, bytesRead %2",
        sysErrorCode, bytesRead);

    if (sysErrorCode == SystemError::noError && bytesRead > 0)
        task->buffer->resize(bufferSizeBak + bytesRead);

    task->status = detail::UserTaskStatus::done;

    nx::utils::swapAndCall(task->handler, sysErrorCode, bytesRead);
}

void StreamTransformingAsyncChannel::processWriteTask(WriteTask* task)
{
    NX_TRACE(this, "Processing write task (%1 bytes)", task->buffer->size());
    NX_ASSERT(isInSelfAioThread());

    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    int bytesWritten = 0;

    const auto rawWriteQueueSizeBak = m_rawWriteQueue.size();

    std::tie(sysErrorCode, bytesWritten) = invokeConverter(
        std::bind(&utils::bstream::Converter::write, m_converter,
            task->buffer->data(),
            task->buffer->size()));
    if (sysErrorCode == SystemError::wouldBlock)
    {
        NX_TRACE(this, "Failed to process write task. wouldBlock");
        return; //< Could not schedule user data send.
    }

    task->status = detail::UserTaskStatus::done;

    NX_TRACE(this, "Write task completed. Result %1, bytesWritten %2",
        sysErrorCode, bytesWritten);

    if (m_rawWriteQueue.size() > rawWriteQueueSizeBak)
    {
        // User data send has been scheduled.
        // Reporting result only after send completion on underlying raw channel.
        NX_ASSERT(!m_rawWriteQueue.empty());
        if (sysErrorCode != SystemError::noError)
        {
            // (SSL) connection has failed, but a reply for the remote peer has been generated.
            // Delivering it, but reporting connection failure in any case.
            m_rawWriteQueue.back().userHandler =
                [sysErrorCode, handler = std::exchange(task->handler, nullptr)](
                    SystemError::ErrorCode /*resultCode*/, std::size_t /*bytesSent*/)
                {
                    handler(sysErrorCode, (std::size_t) -1);
                };
        }
        else
        {
            m_rawWriteQueue.back().userHandler = std::exchange(task->handler, nullptr);
        }
        m_rawWriteQueue.back().userByteCount = task->buffer->size();
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
    m_asyncReadPostponed = false;

    const int result = func();

    issueIoOperationsScheduledByConverter();

    if (result >= 0)
        return std::make_tuple(SystemError::noError, result);

    if (m_converter->failed())
    {
        NX_VERBOSE(this, "Converter reported failure");
        // This is actually converter error. E.g., SSL stream corruption. Not a system error!
        // TODO: #akolesnikov Set converter-specific error code.
        const auto lastErrorCode = SystemError::getLastOSErrorCode();
        return std::make_tuple(
            lastErrorCode != SystemError::noError ? lastErrorCode : SystemError::connectionReset,
            -1);
    }

    if (m_converter->eof())
    {
        NX_TRACE(this, "Converter reported EOF");
        // Correct stream shutdown.
        return std::make_tuple(SystemError::noError, 0);
    }

    // Converter has not moved to "failed" state, so we have a recoverable error here.

    NX_ASSERT(
        result == utils::bstream::StreamIoError::wouldBlock ||
            result == utils::bstream::StreamIoError::osError,
        nx::format("result = %1").args(result));
    return std::make_tuple(SystemError::wouldBlock, -1);
}

void StreamTransformingAsyncChannel::issueIoOperationsScheduledByConverter()
{
    if (m_asyncReadPostponed)
        readRawChannelAsync();

    // Scheduling I/O operations that have been added during the converter invocation.
    if (m_rawWriteQueue.size() == 1 && !m_rawWriteQueue.back().inProgress)
    {
        m_rawWriteQueue.back().inProgress = true;
        if (m_sendShutdown)
            post([this]() { onRawDataWritten(SystemError::connectionReset, (size_t)-1); });
        else
            scheduleNextRawSendTaskIfAny();
    }
}

int StreamTransformingAsyncChannel::readRawBytes(void* data, size_t count)
{
    NX_ASSERT(isInSelfAioThread());

    if (!m_readRawData.empty())
        return readRawDataFromCache(data, count);

    if (!m_asyncReadInProgress)
        m_asyncReadPostponed = true;

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
        memcpy(dataBytes, rawData.data(), bytesToCopy);
        dataBytes += bytesToCopy;
        bytesToReadCount -= bytesToCopy;
        rawData.erase(0, (int)bytesToCopy);
        if (m_readRawData.front().empty())
            m_readRawData.pop_front();
        bytesRead += bytesToCopy;
    }

    NX_TRACE(this, "%1 bytes read from cache. %2 bytes were requested", bytesRead, count);

    return (int)bytesRead;
}

void StreamTransformingAsyncChannel::pause()
{
    m_pauseLevel++;
}

void StreamTransformingAsyncChannel::resume()
{
    if (--m_pauseLevel == 0)
        tryToCompleteUserTasks();
}

void StreamTransformingAsyncChannel::readRawChannelAsync()
{
    static constexpr std::size_t kRawReadBufferSize = 16 * 1024;

    NX_TRACE(this, "Scheduling socket read operation");

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
    reportFailureToTasksFilteredByType(sysErrorCode, detail::UserTaskType::read);
}

int StreamTransformingAsyncChannel::writeRawBytes(const void* data, size_t count)
{
    NX_ASSERT(isInSelfAioThread());

    // TODO: #akolesnikov Put a limit on send queue size.
    if (m_rawWriteQueue.empty() || m_rawWriteQueue.back().inProgress)
        m_rawWriteQueue.push_back(RawSendContext());
    m_rawWriteQueue.back().data.append((const char*)data, (int)count);

    return (int) count;
}

void StreamTransformingAsyncChannel::onRawDataWritten(
    SystemError::ErrorCode resultCode,
    [[maybe_unused]] std::size_t bytesTransferred)
{
    NX_ASSERT(isInSelfAioThread());

    auto completedIoRange =
        std::make_pair(m_rawWriteQueue.begin(), std::next(m_rawWriteQueue.begin()));
    if (resultCode != SystemError::noError)
    {
        // Marking socket unusable since we cannot throw away bytes out of SSL stream.
        m_sendShutdown = true;

        // Considering every queue raw write failed since send has been shut down.
        completedIoRange.second = m_rawWriteQueue.end();
    }

    if (!completeRawSendTasks(takeRawSendTasks(completedIoRange), resultCode))
        return;

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
        reportFailureToTasksFilteredByType(resultCode, detail::UserTaskType::write);
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

bool StreamTransformingAsyncChannel::completeRawSendTasks(
    std::deque<RawSendContext> completedRawSendTasks,
    SystemError::ErrorCode sysErrorCode)
{
    for (auto& sendTask: completedRawSendTasks)
    {
        if (!sendTask.userHandler)
            continue;

        nx::utils::InterruptionFlag::Watcher interruptionWatcher(&m_aioInterruptionFlag);

        try
        {
            nx::utils::swapAndCall(
                sendTask.userHandler,
                sysErrorCode,
                sysErrorCode == SystemError::noError ? sendTask.userByteCount : (size_t) -1);
        }
        catch (const std::exception& e)
        {
            NX_DEBUG(typeid(StreamTransformingAsyncChannel), "%1. Exception caught while reporting "
                "SSL raw send completion. %2", (void*) this, e.what());
        }

        if (interruptionWatcher.interrupted())
            return false;
    }

    return true;
}

void StreamTransformingAsyncChannel::scheduleNextRawSendTaskIfAny()
{
    if (!m_rawWriteQueue.empty())
    {
        NX_TRACE(this, "Scheduling socket write operation");

        m_rawDataChannel->sendAsync(
            &m_rawWriteQueue.front().data,
            [this](auto&&... args) { onRawDataWritten(std::move(args)...); });
        m_rawWriteQueue.front().inProgress = true;
    }
}

void StreamTransformingAsyncChannel::reportFailureOfEveryUserTask(
    SystemError::ErrorCode sysErrorCode)
{
    reportFailureToTasksFilteredByType(sysErrorCode, std::nullopt);
}

void StreamTransformingAsyncChannel::reportFailureToTasksFilteredByType(
    SystemError::ErrorCode sysErrorCode,
    std::optional<detail::UserTaskType> userTypeFilter)
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
        nx::utils::InterruptionFlag::Watcher interruptionWatcher(&m_aioInterruptionFlag);
        if (sysErrorCode == SystemError::noError) //< Connection closed.
        {
            if (userTask->type == detail::UserTaskType::read)
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
        if (it->get()->type == detail::UserTaskType::read &&
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
        else if (it->get()->type == detail::UserTaskType::write &&
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

    if (eventType == aio::EventType::etRead || eventType == aio::EventType::etNone)
        m_readScheduler.cancelPostedCallsSync();

    if (eventType == aio::EventType::etWrite || eventType == aio::EventType::etNone)
        m_sendScheduler.cancelPostedCallsSync();

    if (eventType == aio::EventType::etNone)
    {
        m_aioInterruptionFlag.interrupt();

        // Needed for pleaseStop to work correctly.
        // Should be removed when AbstractStreamSocket inherits BasicPollable.
        m_rawDataChannel->cancelIOSync(aio::EventType::etNone);
    }
}

std::string StreamTransformingAsyncChannel::toString(
    const std::deque<std::shared_ptr<UserTask>>& taskQueue)
{
    std::ostringstream os;
    for (const auto& task: taskQueue)
        os << toString(*task) << "; ";

    return os.str();
}

std::string StreamTransformingAsyncChannel::toString(const UserTask& task)
{
    std::ostringstream os;
    os << "t " << (int)task.type << ", s " << (int)task.status;
    if (task.type == detail::UserTaskType::write)
        os << " (" << nx::utils::toBase64(*static_cast<const WriteTask&>(task).buffer) << ")";

    return os.str();
}

} // namespace nx::network::aio
