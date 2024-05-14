// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <memory>
#include <tuple>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/std/optional.h>

#include "abstract_async_channel.h"

namespace nx::network::aio {

namespace detail {

NX_REFLECTION_ENUM_CLASS(UserTaskType,
    read,
    write
);

NX_REFLECTION_ENUM_CLASS(UserTaskStatus,
    inProgress,
    done
);

} // namespace detail

using UserIoHandler = IoCompletionHandler;

/**
 * Delegates read/write calls to the wrapped AbstractAsyncChannel
 *   moving data through nx::utils::bstream::Converter first.
 * WARNING: Converter MUST NOT generate wouldBlock error by itself before
 *   invoking underlying input/output. Otherwise, behavior is undefined.
 *   Effectively, that means conversion cannot change size of data.
 */
class NX_NETWORK_API StreamTransformingAsyncChannel:
    public AbstractAsyncChannel
{
    using base_type = AbstractAsyncChannel;

public:
    StreamTransformingAsyncChannel(
        std::unique_ptr<AbstractAsyncChannel> rawDataChannel,
        nx::utils::bstream::Converter* converter);
    virtual ~StreamTransformingAsyncChannel() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void readSomeAsync(nx::Buffer* const buffer, UserIoHandler handler) override;
    virtual void sendAsync(const nx::Buffer* buffer, UserIoHandler handler) override;

    int readRawDataFromCache(void* data, size_t count);

    /**
     * Stops handing of any I/O events from the wrapped channel until the
     * corresponding resume() call. Recursive calling is supported.
     */
    void pause();

    /**
     * Resumes previously stopped I/O events handling. Every pause() call
     * must have corresponding resume() call. All deferred events will be
     * handled upon the resumption.
     */
    void resume();

protected:
    virtual void cancelIoInAioThread(aio::EventType eventType) override;

private:
    struct UserTask
    {
        const detail::UserTaskType type;
        UserIoHandler handler;
        detail::UserTaskStatus status = detail::UserTaskStatus::inProgress;

        UserTask(detail::UserTaskType type, UserIoHandler handler):
            type(type),
            handler(std::move(handler))
        {
        }
    };

    struct ReadTask: UserTask
    {
        nx::Buffer* buffer;

        ReadTask(nx::Buffer* const buffer, UserIoHandler handler):
            UserTask(detail::UserTaskType::read, std::move(handler)),
            buffer(buffer)
        {
        }
    };

    struct WriteTask: UserTask
    {
        const nx::Buffer* buffer;

        WriteTask(const nx::Buffer* buffer, UserIoHandler handler):
            UserTask(detail::UserTaskType::write, std::move(handler)),
            buffer(buffer)
        {
        }
    };

    struct RawSendContext
    {
        nx::Buffer data;
        int userByteCount = 0;
        UserIoHandler userHandler;
        bool inProgress = false;
    };

    std::unique_ptr<AbstractAsyncChannel> m_rawDataChannel;
    nx::utils::bstream::Converter* m_converter;
    nx::Buffer m_readBuffer;
    nx::Buffer m_encodedDataBuffer;
    BasicPollable m_readScheduler;
    BasicPollable m_sendScheduler;
    std::function<void(SystemError::ErrorCode, size_t)> m_userReadHandler;
    std::function<void(SystemError::ErrorCode, size_t)> m_userWriteHandler;
    std::unique_ptr<nx::utils::bstream::AbstractInput> m_inputPipeline;
    std::unique_ptr<nx::utils::bstream::AbstractOutput> m_outputPipeline;
    std::deque<std::shared_ptr<UserTask>> m_userTaskQueue;
    nx::Buffer m_rawDataReadBuffer;
    std::deque<nx::Buffer> m_readRawData;
    std::deque<RawSendContext> m_rawWriteQueue;
    bool m_asyncReadInProgress = false;
    bool m_asyncReadPostponed = false;
    nx::utils::InterruptionFlag m_aioInterruptionFlag;
    bool m_sendShutdown = false;
    std::atomic<int> m_pauseLevel = 0;

    virtual void stopWhileInAioThread() override;

    void tryToCompleteUserTasks();
    void tryToCompleteUserTasks(
        const std::deque<std::shared_ptr<UserTask>>& userTaskQueue);
    void processTask(UserTask* task);
    void processReadTask(ReadTask* task);
    void processWriteTask(WriteTask* task);

    template<typename TransformerFunc>
    std::tuple<SystemError::ErrorCode, int /*bytesTransferred*/>
        invokeConverter(TransformerFunc func);

    void issueIoOperationsScheduledByConverter();

    int readRawBytes(void* data, size_t count);
    void readRawChannelAsync();
    void onSomeRawDataRead(SystemError::ErrorCode, std::size_t);
    int writeRawBytes(const void* data, size_t count);

    void onRawDataWritten(SystemError::ErrorCode, std::size_t);
    template<typename Range> std::deque<RawSendContext> takeRawSendTasks(Range range);

    /**
     * @return False if was interrupted. All further processing should be stopped until the next event.
     */
    bool completeRawSendTasks(
        std::deque<RawSendContext> completedRawSendTasks,
        SystemError::ErrorCode sysErrorCode);

    void scheduleNextRawSendTaskIfAny();

    void reportFailureOfEveryUserTask(SystemError::ErrorCode sysErrorCode);
    void reportFailureToTasksFilteredByType(
        SystemError::ErrorCode sysErrorCode,
        std::optional<detail::UserTaskType> userTypeFilter);

    void removeUserTask(UserTask* task);

    static std::string toString(const std::deque<std::shared_ptr<UserTask>>& taskQueue);
    static std::string toString(const UserTask& task);
};

} // namespace nx::network::aio
