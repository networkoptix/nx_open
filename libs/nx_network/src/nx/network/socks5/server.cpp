// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server.h"
#include "messages.h"

#include <unordered_map>

namespace nx::network::socks5 {

class ServerConnection: public AbstractServerConnection
{
    using base_type = AbstractServerConnection;

public:
    ServerConnection(
        std::unique_ptr<nx::network::AbstractStreamSocket> streamSocket,
        std::unique_ptr<AbstractTunnelConnector> connector)
        :
        base_type(std::move(streamSocket)),
        m_creationTimestamp(std::chrono::steady_clock::now()),
        m_connector(std::move(connector))
    {
        m_connector->bindToAioThread(getAioThread());
    }

protected:
    virtual void bytesReceived(const nx::Buffer& buffer) override;
    virtual void readyToSendData() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        if (m_connector)
            m_connector->bindToAioThread(aioThread);
        if (m_bridge)
            m_bridge->bindToAioThread(aioThread);
    }

    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();
        m_connector.reset();
        m_bridge.reset();
    }

private:
    enum class State
    {
        Greet,
        Auth,
        Connect,
        AwaitResponse,
        Proxy,
        Failure
    };

    template<typename T>
    State switchState(
        ServerConnection::State(ServerConnection::*nextState)(const T&));

private:
    State greet(const GreetRequest& request);
    State checkAuth(const AuthRequest& request);
    State makeConnection(const ConnectRequest& request);

    void initiateConnectionTo(const std::string& host, uint16_t port);

    State setupDataProxy(std::unique_ptr<nx::network::AbstractStreamSocket> destinationSocket);

    void abortConnection()
    {
        // Avoid direct call from AIO thread to prevent use-after-free errors when accesing `this`.
        post([this] { closeConnection(SystemError::connectionAbort); });
    }

    uint8_t selectAuthMethod(const GreetRequest& request) const;

    void sendResponseToClient(const Message& message);

    static uint8_t toSocks5Status(SystemError::ErrorCode errorCode)
    {
        static const std::unordered_map<SystemError::ErrorCode, uint8_t> systemToSocks = {
            { SystemError::noError, kSuccess },
            { SystemError::noPermission, kConnectionNotAllowed },
            { SystemError::networkUnreachable, kNetworkUnreachable },
            { SystemError::hostUnreachable, kHostUnreachable },
            { SystemError::hostNotFound, kHostUnreachable },
            { SystemError::connectionRefused, kConnectionRefused },
        };

        const auto it = systemToSocks.find(errorCode);

        // There is no matching system error for kTtlExpired, the SOCKS5 status codes
        // kCommandNotSupported and kAddressTypeNotSupported are reported by the server itself and
        // cannot be mapped from a SystemError of AbstractTunnelConnector, so just return general
        // failure for all these and other unmapped codes.
        return it == systemToSocks.end() ? kGeneralFailure : it->second;
    }

private:
    std::chrono::steady_clock::time_point m_creationTimestamp;

    State m_state = State::Greet;

    std::unique_ptr<AbstractTunnelConnector> m_connector;
    std::unique_ptr<nx::network::aio::AsyncChannelBridge> m_bridge;

    nx::Buffer m_outputBuffer;
    nx::Buffer m_inputBuffer;

    std::unique_ptr<nx::network::AbstractStreamSocket> m_connectResult;

    bool m_isSending = false; //< For safety check: sending one message at a time.
};

void ServerConnection::bytesReceived(const nx::Buffer& buffer)
{
    if (buffer.empty())
        return;

    m_inputBuffer.append(buffer);

    switch (m_state)
    {
        case State::Greet:
            m_state = switchState(&ServerConnection::greet);
            break;
        case State::Connect:
            m_state = switchState(&ServerConnection::makeConnection);
            break;
        case State::Auth:
            m_state = switchState(&ServerConnection::checkAuth);
            break;
        default:
            NX_DEBUG(this, "SOCKS5 Protocol error: invalid state %1", (int) m_state);
            abortConnection();
            break;
    }
}

void ServerConnection::readyToSendData()
{
    m_isSending = false;

    switch (m_state)
    {
        case State::Proxy:
            NX_ASSERT(false, "SOCKS5 server internal state error");
            [[fallthrough]];
        case State::Failure:
            // Wrong state or failure, abort.
            abortConnection();
            return;
        case State::AwaitResponse:
        {
            // Connection success/failure message have been sent to the client, enable data proxy.
            m_state = setupDataProxy(std::move(m_connectResult));
            return;
        }
        default:
            // Message have been sent, do nothing.
            return;
    }
}

ServerConnection::State ServerConnection::setupDataProxy(
    std::unique_ptr<nx::network::AbstractStreamSocket> destinationSocket)
{
    if (!destinationSocket)
    {
        abortConnection();
        return State::Failure;
    }

    m_connector.reset();
    m_inputBuffer.clear();
    m_outputBuffer.clear();

    m_bridge = aio::makeAsyncChannelBridge(takeSocket(), std::move(destinationSocket));

    m_bridge->bindToAioThread(getAioThread());
    m_bridge->start(
        [this](SystemError::ErrorCode resultCode)
        {
            closeConnection(resultCode);
        });

    return State::Proxy;
}

void ServerConnection::sendResponseToClient(const Message& message)
{
    m_outputBuffer = message.toBuffer();
    dispatch(
        [this]()
        {
            NX_ASSERT(!m_isSending);
            m_isSending = true;
            sendBufAsync(&m_outputBuffer);
        });
}

template<typename T>
ServerConnection::State ServerConnection::switchState(
    ServerConnection::State(ServerConnection::*nextState)(const T&))
{
    // Read incoming message until it's completely parsed.
    T request;
    const auto parseResult = request.parse(m_inputBuffer);

    switch (parseResult.status)
    {
        case Message::ParseStatus::Error:
            abortConnection();
            return State::Failure;
        case Message::ParseStatus::NeedMoreData:
            return m_state; //< Do not change state.
        case Message::ParseStatus::Complete:
            break;
    }

    // We should get only one message from the client, otherwise this is a protocol failure.
    if (m_inputBuffer.size() != parseResult.size)
    {
        abortConnection();
        return State::Failure;
    }
    m_inputBuffer.clear();

    connectionStatistics.messageReceived();

    return (this->*nextState)(request);
}

uint8_t ServerConnection::selectAuthMethod(const GreetRequest& request) const
{
    const auto requiredMethod = m_connector->hasAuth() ? kMethodUserPass : kMethodNoAuth;
    const auto& methods = request.methods;

    return std::find(methods.begin(), methods.end(), requiredMethod) == methods.end()
        ? kMethodUnacceptable
        : requiredMethod;
}

ServerConnection::State ServerConnection::greet(const GreetRequest& request)
{
    // Read greeting message which contains authentication methods supported by the client.

    // Check for supported authentication methods and select the first one matching.
    const uint8_t authMethod = selectAuthMethod(request);

    // Respond to the client with selected authentication method.
    sendResponseToClient(GreetResponse(authMethod));

    switch (authMethod)
    {
        case kMethodUserPass:
            return State::Auth;
        case kMethodNoAuth:
            return State::Connect;
        case kMethodUnacceptable:
        default:
            return State::Failure;
    }
}

ServerConnection::State ServerConnection::checkAuth(const AuthRequest& request)
{
    // The client is expected to send authentication message which contains username and password.

    // Verify the credentials.
    const bool authSuccess = m_connector->auth(request.username, request.password);

    // Respond to the client with success or failure.
    sendResponseToClient(AuthResponse(authSuccess ? kSuccess : kGeneralFailure));

    return authSuccess ? State::Connect : State::Failure;
}

ServerConnection::State ServerConnection::makeConnection(
    const ConnectRequest& request)
{
    // After greeting and authentication (if required) the client sends connection request.

    if (request.command != kCommandTcpConnection)
    {
        NX_DEBUG(this, "Unsupported connect command %1", request.command);
        sendResponseToClient(ConnectResponse(kCommandNotSupported, request.host, request.port));
        return State::Failure;
    }

    // Initiate outgoing connection and start waiting for result.
    initiateConnectionTo(request.host, request.port);

    return State::AwaitResponse;
}

void ServerConnection::initiateConnectionTo(const std::string& host, uint16_t port)
{
    // This method is called in success or failure cases, socket is nullptr in case of failure.
    const auto onConnectionDone =
        [this, host, port](
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractStreamSocket> socket)
        {
            m_connectResult = std::move(socket);
            const uint8_t status = toSocks5Status(errorCode);

            sendResponseToClient(ConnectResponse(status, host, port));
        };

    m_connector->connectTo(SocketAddress(host, port), onConnectionDone);
}

std::shared_ptr<AbstractServerConnection> Server::createConnection(
    std::unique_ptr<AbstractStreamSocket> streamSocket)
{
    return std::make_shared<ServerConnection>(std::move(streamSocket), m_connectorFactoryFunc());
}

} // namespace nx::network::socks5
