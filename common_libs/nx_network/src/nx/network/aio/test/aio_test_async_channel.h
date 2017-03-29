#pragma once

#include <atomic>
#include <memory>

#include <nx/network/aio/abstract_async_channel.h>
#include <nx/utils/pipeline.h>
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
    };

    AsyncChannel(
        utils::pipeline::AbstractInput* input,
        utils::pipeline::AbstractOutput* output,
        InputDepletionPolicy inputDepletionPolicy);
    virtual ~AsyncChannel() override;

    virtual void bindToAioThread(AbstractAioThread* aioThread) override;
    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override;
    virtual void cancelIOSync(EventType eventType) override;
    void pauseReadingData();
    void resumeReadingData();
    void pauseSendingData();
    void resumeSendingData();
    void waitForSomeDataToBeRead();
    void waitForReadSequenceToBreak();
    QByteArray dataRead() const;
    void setErrorState();

private:
    utils::pipeline::AbstractInput* m_input;
    utils::pipeline::AbstractOutput* m_output;
    InputDepletionPolicy m_inputDepletionPolicy;
    std::atomic<bool> m_errorState;
    std::atomic<std::size_t> m_totalBytesRead;
    mutable QnMutex m_mutex;
    QByteArray m_totalDataRead;

    std::function<void(SystemError::ErrorCode, size_t)> m_readHandler;
    bool m_readPaused;
    nx::Buffer* m_readBuffer;

    std::function<void(SystemError::ErrorCode, size_t)> m_sendHandler;
    bool m_sendPaused;
    const nx::Buffer* m_sendBuffer;

    BasicPollable m_reader;
    BasicPollable m_writer;
    bool m_readPosted;
    std::atomic<int> m_readSequence;

    virtual void stopWhileInAioThread() override;

    void handleInputDepletion(
        std::function<void(SystemError::ErrorCode, size_t)> handler);
    void performAsyncRead(const QnMutexLockerBase& /*lock*/);
    void performAsyncSend(const QnMutexLockerBase&);
};

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
