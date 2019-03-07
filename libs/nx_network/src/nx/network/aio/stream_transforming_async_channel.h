#pragma once

#include <deque>
#include <memory>
#include <tuple>

#include <nx/network/aio/interruption_flag.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/std/optional.h>

#include "abstract_async_channel.h"

namespace nx {
namespace network {
namespace aio {

using UserIoHandler = IoCompletionHandler;

/**
 * Delegates read/write calls to the wrapped AbstractAsyncChannel
 *   moving data through utils::bstream::Converter first.
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
    virtual void sendAsync(const nx::Buffer& buffer, UserIoHandler handler) override;

    int readRawDataFromCache(void* data, size_t count);

protected:
    virtual void cancelIoInAioThread(aio::EventType eventType) override;

private:
    enum class UserTaskType
    {
        read,
        write,
    };

    enum UserTaskStatus
    {
        inProgress,
        done,
    };

    struct UserTask
    {
        UserTaskType type;
        UserIoHandler handler;
        UserTaskStatus status;

        UserTask(UserTaskType type, UserIoHandler handler):
            type(type),
            handler(std::move(handler)),
            status(UserTaskStatus::inProgress)
        {
        }
    };

    struct ReadTask: UserTask
    {
        nx::Buffer* buffer;

        ReadTask(nx::Buffer* const buffer, UserIoHandler handler):
            UserTask(UserTaskType::read, std::move(handler)),
            buffer(buffer)
        {
        }
    };

    struct WriteTask: UserTask
    {
        const nx::Buffer& buffer;

        WriteTask(const nx::Buffer& buffer, UserIoHandler handler):
            UserTask(UserTaskType::write, std::move(handler)),
            buffer(buffer)
        {
        }
    };

    struct RawSendContext
    {
        nx::Buffer data;
        int userByteCount = 0;
        UserIoHandler userHandler;
    };

    std::unique_ptr<AbstractAsyncChannel> m_rawDataChannel;
    nx::utils::bstream::Converter* m_converter;
    nx::Buffer m_readBuffer;
    nx::Buffer m_encodedDataBuffer;
    std::function<void(SystemError::ErrorCode, size_t)> m_userReadHandler;
    std::function<void(SystemError::ErrorCode, size_t)> m_userWriteHandler;
    std::unique_ptr<utils::bstream::AbstractInput> m_inputPipeline;
    std::unique_ptr<utils::bstream::AbstractOutput> m_outputPipeline;
    std::deque<std::shared_ptr<UserTask>> m_userTaskQueue;
    nx::Buffer m_rawDataReadBuffer;
    std::deque<nx::Buffer> m_readRawData;
    std::deque<RawSendContext> m_rawWriteQueue;
    bool m_asyncReadInProgress;
    InterruptionFlag m_aioInterruptionFlag;
    bool m_sendShutdown = false;

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

    int readRawBytes(void* data, size_t count);
    void readRawChannelAsync();
    void onSomeRawDataRead(SystemError::ErrorCode, std::size_t);
    int writeRawBytes(const void* data, size_t count);

    void onRawDataWritten(SystemError::ErrorCode, std::size_t);
    template<typename Range> std::deque<RawSendContext> takeRawSendTasks(Range range);

    /**
     * @return false if was interrupted. All futher processing should be stopped until the next event.
     */
    bool completeRawSendTasks(
        std::deque<RawSendContext> completedRawSendTasks,
        SystemError::ErrorCode sysErrorCode);

    void scheduleNextRawSendTaskIfAny();

    void reportFailureOfEveryUserTask(SystemError::ErrorCode sysErrorCode);
    void reportFailureToTasksFilteredByType(
        SystemError::ErrorCode sysErrorCode,
        std::optional<UserTaskType> userTypeFilter);

    void removeUserTask(UserTask* task);
};

} // namespace aio
} // namespace network
} // namespace nx
