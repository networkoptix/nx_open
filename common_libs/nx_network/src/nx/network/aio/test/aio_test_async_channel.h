#pragma once

#include <atomic>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class NX_NETWORK_API AsyncChannel:
    public AbstractAsyncChannel
{
    using base_type = BasicPollable;

public:
    enum class InputDepletionPolicy
    {
        sendConnectionReset,
        ignore,
        retry,
    };

    AsyncChannel(
        utils::bstream::AbstractInput* input,
        utils::bstream::AbstractOutput* output,
        InputDepletionPolicy inputDepletionPolicy);
    virtual ~AsyncChannel() override;

    virtual void bindToAioThread(AbstractAioThread* aioThread) override;
    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer& buffer,
        IoCompletionHandler handler) override;
    virtual void cancelIOSync(EventType eventType) override;

    void pauseReadingData();
    void resumeReadingData();
    void pauseSendingData();
    void resumeSendingData();
    void waitForSomeDataToBeRead();
    void waitForReadSequenceToBreak();
    QByteArray dataRead() const;
    void setErrorState();
    void setSendErrorState(boost::optional<SystemError::ErrorCode> sendErrorCode);
    void setReadErrorState(boost::optional<SystemError::ErrorCode> sendErrorCode);

    void waitForAnotherReadErrorReported();
    void waitForAnotherSendErrorReported();

    bool isReadScheduled() const;
    bool isWriteScheduled() const;

private:
    utils::bstream::AbstractInput* m_input;
    utils::bstream::AbstractOutput* m_output;
    InputDepletionPolicy m_inputDepletionPolicy;
    boost::optional<SystemError::ErrorCode> m_readErrorState;
    boost::optional<SystemError::ErrorCode> m_sendErrorState;
    std::atomic<std::size_t> m_totalBytesRead;
    mutable QnMutex m_mutex;
    QByteArray m_totalDataRead;
    nx::utils::ObjectDestructionFlag m_destructionFlag;

    IoCompletionHandler m_readHandler;
    bool m_readPaused;
    nx::Buffer* m_readBuffer;

    IoCompletionHandler m_sendHandler;
    bool m_sendPaused;
    const nx::Buffer* m_sendBuffer;

    BasicPollable m_reader;
    BasicPollable m_writer;
    bool m_readScheduled;
    std::atomic<int> m_readSequence;
    std::atomic<int> m_readErrorsReported;
    std::atomic<int> m_sendErrorsReported;

    virtual void stopWhileInAioThread() override;

    void handleInputDepletion();
    void performAsyncRead(const QnMutexLockerBase& /*lock*/);
    void performAsyncSend(const QnMutexLockerBase&);
    void reportIoCompletion(
        IoCompletionHandler* ioCompletionHandler,
        SystemError::ErrorCode sysErrorCode,
        std::size_t bytesTransferred);
};

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
