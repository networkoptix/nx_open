// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>

#include <nx/network/aio/timer.h>
#include <nx/network/http/http_parser.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/p2p/transport/i_p2p_transport.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/system_error.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/buffer.h>

namespace nx::p2p {

class P2PHttpServerTransport: public IP2PTransport
{
public:
    P2PHttpServerTransport(
        std::unique_ptr<network::AbstractStreamSocket> socket,
        network::websocket::FrameType messageType,
        std::optional<std::chrono::milliseconds> pingTimeout);

    virtual ~P2PHttpServerTransport() override;

    void gotPostConnection(
        std::unique_ptr<network::AbstractStreamSocket> socket,
        nx::network::http::Request request);

    /**
     * This should be called before all other operations. Transport becomes operational only after
     * onGetRequestReceived() callback is fired.
     */
    virtual void start(
        utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onGetRequestReceived) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        network::IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer* buffer,
        network::IoCompletionHandler handler) override;
    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual network::aio::AbstractAioThread* getAioThread() const override;
    virtual network::SocketAddress getForeignAddress() const override;

    // For tests.
    static void setPingTimeout(std::optional<std::chrono::milliseconds> pingTimeout);
    static void setPingPongDisabled(bool value);

    virtual QString lastErrorMessage() const override;

private:
    enum Headers
    {
        contentType = 0x1,
        ping = 0x2
    };

    struct OutgoingData
    {
        std::optional<nx::Buffer> buffer;
        network::IoCompletionHandler handler;
        int headerMask = 0;
    };

    using UserReadHandlerPair =
        std::unique_ptr<std::pair<nx::Buffer* const, network::IoCompletionHandler>>;

    std::unique_ptr<network::AbstractStreamSocket> m_sendSocket;
    std::unique_ptr<network::AbstractStreamSocket> m_readSocket;
    network::websocket::FrameType m_messageType;
    nx::Buffer m_sendBuffer;
    nx::Buffer m_responseBuffer;
    nx::Buffer m_providedPostBody;
    nx::Buffer m_sendChannelReadBuffer;
    nx::Buffer m_readBuffer;
    bool m_firstSend = true;
    network::aio::Timer m_pingTimer;
    network::aio::Timer m_inactivityTimer;
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onGetRequestReceived = nullptr;
    bool m_failed = false;
    utils::InterruptionFlag m_destructionFlag;
    UserReadHandlerPair m_userReadHandlerPair;
    const std::optional<std::chrono::milliseconds> m_pingTimeout;
    std::queue<OutgoingData> m_outgoingMessageQueue;
    bool m_sendInProgress = false;
    network::http::MessageParser m_httpParser;
    network::http::Message m_httpMessage;
    QString m_lastErrorMessage;

    // For tests.
    static std::optional<std::chrono::milliseconds> s_pingTimeout;
    static bool s_pingPongDisabled;

    void onIncomingPost(nx::network::http::Request request);
    nx::Buffer makeInitialResponse() const;
    nx::Buffer makeFrameHeader(int headers, int length) const;
    void sendPostResponse(network::IoCompletionHandler handler);
    void onReadFromSendSocket(SystemError::ErrorCode error, size_t transferred);
    void initiatePing();
    void initiateInactivityTimer();
    std::optional<std::chrono::milliseconds> pingTimeout() const;
    void setFailedState(SystemError::ErrorCode errorCode, const QString& message);
    void sendPing();
    void sendNextMessage();
    void onRead(SystemError::ErrorCode error, size_t transferred);

    static void addDateHeader(network::http::HttpHeaders* headers);

    virtual void stopWhileInAioThread() override;
};

} // namespace nx::network
