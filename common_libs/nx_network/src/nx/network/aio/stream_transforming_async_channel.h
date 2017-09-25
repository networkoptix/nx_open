#pragma once

#include <deque>
#include <memory>
#include <tuple>

#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/byte_stream/pipeline.h>

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
    virtual void cancelIOSync(aio::EventType eventType) override;

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

    std::unique_ptr<AbstractAsyncChannel> m_rawDataChannel;
    nx::utils::bstream::Converter* m_converter;
    nx::Buffer* m_userReadBuffer;
    nx::Buffer m_readBuffer;
    nx::Buffer m_encodedDataBuffer;
    std::size_t m_bytesEncodedOnPreviousStep;
    std::function<void(SystemError::ErrorCode, size_t)> m_userReadHandler;
    std::function<void(SystemError::ErrorCode, size_t)> m_userWriteHandler;
    std::unique_ptr<utils::bstream::AbstractInput> m_inputPipeline;
    std::unique_ptr<utils::bstream::AbstractOutput> m_outputPipeline;
    std::deque<std::unique_ptr<UserTask>> m_userTaskQueue;
    nx::Buffer m_rawDataReadBuffer;
    std::deque<nx::Buffer> m_readRawData;
    std::deque<nx::Buffer> m_rawWriteQueue;
    bool m_asyncReadInProgress;
    nx::utils::ObjectDestructionFlag m_destructionFlag;

    virtual void stopWhileInAioThread() override;

    void tryToCompleteUserTasks();
    void processTask(UserTask* task);
    void processReadTask(ReadTask* task);
    void processWriteTask(WriteTask* task);
    template<typename TransformerFunc>
    std::tuple<SystemError::ErrorCode, int /*bytesTransferred*/>
        invokeConverter(TransformerFunc func);

    int readRawBytes(void* data, size_t count);
    int readRawDataFromCache(void* data, size_t count);
    void readRawChannelAsync();
    void onSomeRawDataRead(SystemError::ErrorCode, std::size_t);
    int writeRawBytes(const void* data, size_t count);
    void onRawDataWritten(SystemError::ErrorCode, std::size_t);
    void handleIoError(SystemError::ErrorCode sysErrorCode);

    void removeUserTask(UserTask* task);
    void cancelIoWhileInAioThread(aio::EventType eventType);
};

} // namespace aio
} // namespace network
} // namespace nx
