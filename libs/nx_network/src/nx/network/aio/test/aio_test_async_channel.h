// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <tuple>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/interruption_flag.h>
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

    using IoState = std::tuple<SystemError::ErrorCode, size_t /*bytes*/>;

    AsyncChannel(
        nx::utils::bstream::AbstractInput* input,
        nx::utils::bstream::AbstractOutput* output,
        InputDepletionPolicy inputDepletionPolicy);
    virtual ~AsyncChannel() override;

    virtual void bindToAioThread(AbstractAioThread* aioThread) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer* buffer,
        IoCompletionHandler handler) override;

    void pauseReadingData();
    void resumeReadingData();
    void pauseSendingData();
    void resumeSendingData();
    void waitForSomeDataToBeRead();
    void waitForReadSequenceToBreak();
    nx::Buffer dataRead() const;
    void setErrorState();
    void setSendErrorState(std::optional<IoState> sendErrorCode);
    void setSendErrorState(SystemError::ErrorCode errorCode);
    void setReadErrorState(std::optional<IoState> sendErrorCode);
    void setReadErrorState(SystemError::ErrorCode errorCode);

    void waitForAnotherReadErrorReported();
    void waitForAnotherSendErrorReported();

    bool isReadScheduled() const;
    bool isWriteScheduled() const;

protected:
    virtual void cancelIoInAioThread(EventType eventType) override;

private:
    nx::utils::bstream::AbstractInput* m_input;
    nx::utils::bstream::AbstractOutput* m_output;
    InputDepletionPolicy m_inputDepletionPolicy;
    std::optional<IoState> m_readErrorState;
    std::optional<IoState> m_sendErrorState;
    std::atomic<std::size_t> m_totalBytesRead;
    mutable nx::Mutex m_mutex;
    nx::Buffer m_totalDataRead;
    nx::utils::InterruptionFlag m_destructionFlag;

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
    void performAsyncRead(const nx::Locker<nx::Mutex>& /*lock*/);
    void performAsyncSend(const nx::Locker<nx::Mutex>&);
    void reportIoCompletion(
        IoCompletionHandler* ioCompletionHandler,
        SystemError::ErrorCode sysErrorCode,
        std::size_t bytesTransferred);
};

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
