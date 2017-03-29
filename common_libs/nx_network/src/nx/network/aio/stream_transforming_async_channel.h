#pragma once

#include <deque>
#include <memory>
#include <tuple>

#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/pipeline.h>

#include "abstract_async_channel.h"

namespace nx {
namespace network {
namespace aio {

using UserIoHandler = std::function<void(SystemError::ErrorCode, size_t)>;

class NX_NETWORK_API StreamTransformingAsyncChannel:
    public AbstractAsyncChannel
{
    using base_type = AbstractAsyncChannel;

public:
    StreamTransformingAsyncChannel(
        std::unique_ptr<AbstractAsyncChannel> rawDataChannel,
        std::unique_ptr<nx::utils::pipeline::Converter> converter);
    virtual ~StreamTransformingAsyncChannel() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void readSomeAsync(nx::Buffer* const buffer, UserIoHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, UserIoHandler handler) override;
    virtual void cancelIOSync(nx::network::aio::EventType eventType) override;

protected:
    virtual void stopWhileInAioThread() override;

    virtual int readRawBytes(void* data, size_t count);
    virtual int writeRawBytes(const void* data, size_t count);

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
    std::unique_ptr<nx::utils::pipeline::Converter> m_converter;
    nx::Buffer* m_userReadBuffer;
    nx::Buffer m_readBuffer;
    nx::Buffer m_encodedDataBuffer;
    std::size_t m_bytesEncodedOnPreviousStep;
    std::function<void(SystemError::ErrorCode, size_t)> m_userReadHandler;
    std::function<void(SystemError::ErrorCode, size_t)> m_userWriteHandler;
    std::unique_ptr<utils::pipeline::AbstractInput> m_inputPipeline;
    std::unique_ptr<utils::pipeline::AbstractOutput> m_outputPipeline;
    std::deque<std::unique_ptr<UserTask>> m_userTaskQueue;
    nx::Buffer m_rawDataReadBuffer;
    std::deque<nx::Buffer> m_readRawData;
    std::deque<nx::Buffer> m_rawWriteQueue;
    bool m_asyncReadInProgress;
    nx::utils::ObjectDestructionFlag m_destructionFlag;

    void readRawChannelAsync();
    int readRawDataFromCache(void* data, size_t count);

    void tryToCompleteNextUserTask();
    void processTask(UserTask* task);
    void processReadTask(ReadTask* task);
    void processWriteTask(WriteTask* task);

    template<typename TransformerFunc>
        void processUserTask(TransformerFunc func, UserTask* task);
    template<typename TransformerFunc>
    std::tuple<SystemError::ErrorCode, int /*bytesTransferred*/>
        invokeTransformer(TransformerFunc func);
    void onSomeRawDataRead(SystemError::ErrorCode, std::size_t);
    void onRawDataWritten(SystemError::ErrorCode, std::size_t);
    void handleIoError(SystemError::ErrorCode sysErrorCode);
};

} // namespace aio
} // namespace network
} // namespace nx
