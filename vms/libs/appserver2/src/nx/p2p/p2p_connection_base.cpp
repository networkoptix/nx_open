// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "p2p_connection_base.h"

#include <chrono>

#include <QtCore/QUrlQuery>

#include <common/static_common_module.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/p2p/p2p_fwd.h>
#include <nx/p2p/p2p_serialization.h>
#include <nx/p2p/transport/p2p_http_client_transport.h>
#include <nx/p2p/transport/p2p_http_server_transport.h>
#include <nx/p2p/transport/p2p_websocket_transport.h>
#include <nx/utils/log/format.h>
#include <transaction/transaction_message_bus_base.h>

// For debug purpose only
//#define CHECK_SEQUENCE

namespace nx {
namespace p2p {

SendCounters ConnectionBase::m_sendCounters = {};

bool ConnectionBase::s_noPingSupportClientHeader = false;

#if defined(CHECK_SEQUENCE)
    const int kMessageOffset = 8;
#else
    const int kMessageOffset = 0;
#endif

QString toString(ConnectionBase::State value)
{
    switch (value)
    {
    case ConnectionBase::State::NotDefined:
        return "NotDefined";
    case ConnectionBase::State::Connecting:
        return "Connecting";
    case ConnectionBase::State::Connected:
        return "Connected";
    case ConnectionBase::State::Error:
        return "Error";
    case ConnectionBase::State::Unauthorized:
        return "Unauthorized";
    case ConnectionBase::State::forbidden:
        return "Forbidden";
    case ConnectionBase::State::handshakeError:
        return "handshakeError";
    default:
        NX_ASSERT(false, "Unknown enum value");
        return "Unknown";
    }
}

ConnectionBase::ConnectionBase(
    const QnUuid& remoteId,
    nx::vms::api::PeerType remotePeerType,
    const vms::api::PeerDataEx& localPeer,
    const nx::utils::Url& remotePeerUrl,
    const std::chrono::seconds& keepAliveTimeout,
    std::unique_ptr<QObject> opaqueObject,
    network::ssl::AdapterFunc adapterFunc,
    std::unique_ptr<ConnectionLockGuard> connectionLockGuard)
    :
    m_direction(Direction::outgoing),
    m_httpClient(std::make_unique<network::http::AsyncClient>(std::move(adapterFunc))),
    m_localPeer(localPeer),
    m_remotePeerUrl(remotePeerUrl),
    m_keepAliveTimeout(keepAliveTimeout),
    m_opaqueObject(std::move(opaqueObject)),
    m_connectionLockGuard(std::move(connectionLockGuard)),
    m_isClient(true)
{
    m_remotePeer.id = remoteId;
    NX_ASSERT(m_localPeer.id != m_remotePeer.id);
    m_httpClient->setSendTimeout(keepAliveTimeout);
    m_httpClient->setResponseReadTimeout(keepAliveTimeout);
    if (remotePeerType != nx::vms::api::PeerType::cloudServer
        || m_remotePeerUrl.scheme() != nx::network::http::kSecureUrlSchemeName)
    {
        m_httpClient->setAuthType(nx::network::http::AuthType::authDigest);
    }

    m_httpClient->bindToAioThread(m_timer.getAioThread());
    NX_DEBUG(this, "Created client p2p connection");
}

ConnectionBase::ConnectionBase(
    const vms::api::PeerDataEx& remotePeer,
    const vms::api::PeerDataEx& localPeer,
    P2pTransportPtr p2pTransport,
    const QUrlQuery& requestUrlQuery,
    std::unique_ptr<QObject> opaqueObject,
    std::unique_ptr<ConnectionLockGuard> connectionLockGuard)
:
    m_direction(Direction::incoming),
    m_remotePeer(remotePeer),
    m_localPeer(localPeer),
    m_p2pTransport(std::move(p2pTransport)),
    m_state(State::Connected),
    m_opaqueObject(std::move(opaqueObject)),
    m_connectionLockGuard(std::move(connectionLockGuard)),
    m_isClient(false)
{
    NX_ASSERT(m_localPeer.id != m_remotePeer.id);

    m_timer.bindToAioThread(m_p2pTransport->getAioThread());

    const auto& queryItems = requestUrlQuery.queryItems();
    std::transform(
        queryItems.begin(), queryItems.end(),
        std::inserter(m_remoteQueryParams, m_remoteQueryParams.end()),
        [](const QPair<QString, QString>& item)
            { return std::make_pair(item.first, item.second); });

    NX_DEBUG(this, "Created server p2p connection");
}

void ConnectionBase::gotPostConnection(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    nx::network::http::Request request)
{
    m_timer.post(
        [this, socket = std::move(socket), request = std::move(request)]() mutable
        {
            using namespace nx::network;
            if(auto httpTransport = dynamic_cast<P2PHttpServerTransport*>(m_p2pTransport.get()))
            {
                httpTransport->gotPostConnection(std::move(socket), std::move(request));
            }
            else
            {
                // TODO: send HTTP error here
            }
        });
}

void ConnectionBase::stopWhileInAioThread()
{
    // All objects in the same AIO thread
    m_timer.pleaseStopSync();
    m_p2pTransport.reset();
    m_httpClient.reset();
}

void ConnectionBase::pleaseStopSync()
{
    if (m_startedClassId)
    {
        NX_ASSERT(m_startedClassId == typeid(*this).hash_code(),
            "Please call pleaseStopSync() in the destructor of the nested class.");
    }

    m_timer.executeInAioThreadSync([this]() { stopWhileInAioThread(); });
}

ConnectionBase::~ConnectionBase()
{
}

nx::utils::Url ConnectionBase::remoteAddr() const
{
    if (m_direction == Direction::outgoing)
        return m_remotePeerUrl;
    if (m_p2pTransport)
    {
        auto address = m_p2pTransport->getForeignAddress();
        return nx::utils::Url(nx::format("http://%1").args(address));
    }
    return nx::utils::Url();
}

void ConnectionBase::addAdditionalRequestHeaders(
    nx::network::http::HttpHeaders headers)
{
    m_additionalRequestHeaders = std::move(headers);
}

void ConnectionBase::addRequestQueryParams(
    std::vector<std::pair<QString, QString>> queryParams)
{
    m_requestQueryParams = std::move(queryParams);
}

void ConnectionBase::cancelConnecting(State newState, const QString& reason)
{
    NX_DEBUG(
        this,
        lit("Connection to peer %1 canceled from state %2. Reason: %3")
        .arg(m_remotePeer.id.toString())
        .arg(toString(state()))
        .arg(reason));
    m_lastErrorMessage = reason;
    setState(newState);
}

QString ConnectionBase::idForToStringFromPtr() const
{
    return remotePeer().id.toString();
}

void ConnectionBase::onHttpClientDone()
{
    if (m_httpClient->failed())
    {
        const auto lastSysErrorCode = m_httpClient->lastSysErrorCode();
        cancelConnecting(
            lastSysErrorCode == SystemError::sslHandshakeError
                ? State::handshakeError
                : State::Error,
            nx::format("Http request failed %1").arg(lastSysErrorCode));

        return;
    }

    const int statusCode = m_httpClient->response()->statusLine.statusCode;

    NX_VERBOSE(this, lit("%1. statusCode = %2").arg(Q_FUNC_INFO).arg(statusCode));

    if (statusCode == nx::network::http::StatusCode::unauthorized)
    {
        // try next credential source
        m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
        using namespace std::placeholders;
        if (m_httpClient->url().userName().isEmpty()
            && m_credentialsSource < CredentialsSource::none
            && fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey))
        {
            m_httpClient->doGet(
                m_httpClient->url(),
                std::bind(&ConnectionBase::onHttpClientDone, this));
        }
        else
        {
            cancelConnecting(State::Unauthorized, nx::format("Unauthorized"));
        }
        return;
    }

    const auto& headers = m_httpClient->response()->headers;
    if (m_connectionLockGuard && headers.find(Qn::EC2_CONNECT_STAGE_1) != headers.end())
    {
        // Addition stage for server to server connect. It prevents to open two (incoming and outgoing) connections at once.
        if (!nx::network::http::StatusCode::isSuccessCode(statusCode)) //< Checking that statusCode is 2xx.
        {
            cancelConnecting(State::Error, nx::format("Not a successful HTTP status code %1").arg(statusCode));
            return;
        }

        if (m_connectionLockGuard->tryAcquireConnecting())
        {
            auto url = m_httpClient->url();
            // Switch to the HTTPS in case of previous request return 'moved' to the HTTPS.
            if (m_httpClient->contentLocationUrl().scheme() == nx::network::http::kSecureUrlSchemeName)
                url.setScheme(nx::network::http::kSecureUrlSchemeName);

            m_httpClient->doGet(
                url,
                std::bind(&ConnectionBase::onHttpClientDone, this));
        }
        else
        {
            cancelConnecting(State::Error, nx::format("tryAcquireConnecting failed"));
        }
        return;
    }

    if (statusCode == nx::network::http::StatusCode::forbidden)
    {
        cancelConnecting(
            State::forbidden,
            nx::format("Remote peer forbid connection with message: %1")
            .arg(m_httpClient->fetchMessageBodyBuffer()));
        return;
    }

    vms::api::PeerDataEx remotePeer = deserializePeerData(headers, m_localPeer.dataFormat);

    if (remotePeer.id.isNull())
    {
        cancelConnecting(State::Error, nx::format("Remote peer Id is null"));
        return;
    }
    else if (remotePeer.id != m_remotePeer.id)
    {
        cancelConnecting(
            State::Error,
            nx::format("Remote peer id %1 is not match expected peer id %2")
            .arg(remotePeer.id.toString())
            .arg(m_remotePeer.id.toString()));
        return;
    }
    else if (!validateRemotePeerData(remotePeer))
    {
        cancelConnecting(
            State::Error,
            nx::format("Remote peer id %1 has inappropriate data to make connection.")
            .arg(remotePeer.id.toString()));
        return;
    }

    m_remotePeer = remotePeer;

    NX_ASSERT(!m_remotePeer.instanceId.isNull());

    if (m_connectionLockGuard && !m_connectionLockGuard->tryAcquireConnected())
    {
        cancelConnecting(State::Error, nx::format("tryAcquireConnected failed"));
        return;
    }

    using namespace nx::network;
    using namespace nx::vms::api;

    auto error = websocket::validateResponse(m_httpClient->request(), *m_httpClient->response());
    auto useWebsocketMode = error == websocket::Error::noError;
    if (!useWebsocketMode)
    {
        NX_WARNING(this,
            nx::format("Can't establish WEB socket connection. Validation failed. Error: %1. Switch to the HTTP mode").arg((int)error));
    }

    using namespace nx::network;
    websocket::FrameType frameType = remotePeer.dataFormat == Qn::JsonFormat
        ? websocket::FrameType::text
        : websocket::FrameType::binary;

    auto compressionType = websocket::compressionType(m_httpClient->response()->headers);
    const auto pingSupportedHeaderIt = headers.find(Qn::EC2_PING_ENABLED_HEADER_NAME);
    const auto pingSupported =
        !s_noPingSupportClientHeader
        && !useWebsocketMode
        && pingSupportedHeaderIt != headers.cend()
        && pingSupportedHeaderIt->second == "true";

    NX_DEBUG(
        this, "Ping supported: %1. useWebsocket: %2, "
        "ping supported header present: %3, header value: %4",
        pingSupported,
        useWebsocketMode,
        pingSupportedHeaderIt != headers.cend(),
        pingSupportedHeaderIt != headers.cend()
            ? pingSupportedHeaderIt->second
            : "");

    if (useWebsocketMode)
    {
        NX_DEBUG(this, "Using websocket p2p transport for connection with '%1'", m_httpClient->url());
        auto socket = m_httpClient->takeSocket();
        socket->setNonBlockingMode(true);
        m_p2pTransport.reset(new P2PWebsocketTransport(
            std::move(socket), nx::network::websocket::Role::client, frameType, compressionType,
            m_keepAliveTimeout));
    }
    else
    {
        NX_DEBUG(this, "Using http p2p transport for connection with '%1'", m_httpClient->url());
        auto url = m_httpClient->url();
        auto urlPath = url.path().replace(kHttpHandshakeUrlPath, kHttpDataUrlPath);
        urlPath.replace(kWebsocketUrlPath, kHttpDataUrlPath);
        url.setPath(urlPath);

        m_p2pTransport.reset(new P2PHttpClientTransport(
            std::move(m_httpClient),
            m_connectionGuid,
            network::websocket::FrameType::binary,
            url,
            pingSupported
                ? std::optional<std::chrono::milliseconds>(kPingTimeout)
                : std::nullopt));
    }

    m_httpClient.reset();
    m_p2pTransport->bindToAioThread(m_timer.getAioThread());
    m_p2pTransport->start([this](SystemError::ErrorCode errorCode)
        {
            if (errorCode == SystemError::noError)
            {
                setState(State::Connected);
            }
            else
            {
                cancelConnecting(
                    errorCode == SystemError::sslHandshakeError
                    ? State::handshakeError
                    : State::Error,
                    nx::format("P2P Http transport connection failed %1").arg(errorCode));
            }
        });
}

void ConnectionBase::setNoPingSupportClientHeader(bool value)
{
    s_noPingSupportClientHeader = value;
}

void ConnectionBase::startConnection()
{
    m_startedClassId = typeid(*this).hash_code();

    auto headers = m_additionalRequestHeaders;
    if (m_remotePeerUrl.path().contains(kWebsocketUrlPath))
    {
        nx::network::websocket::addClientHeaders(
            &headers, kP2pProtoName, nx::network::websocket::CompressionType::perMessageDeflate);
    }
    m_connectionGuid = QnUuid::createUuid().toByteArray();
    headers.emplace(Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_connectionGuid);
    if (!s_noPingSupportClientHeader)
        headers.emplace(Qn::EC2_PING_ENABLED_HEADER_NAME, "true");

    m_httpClient->addRequestHeaders(headers);

    auto requestUrl = m_remotePeerUrl;
    QUrlQuery requestUrlQuery(requestUrl.query());
    for (const auto& param: m_requestQueryParams)
        requestUrlQuery.addQueryItem(param.first, param.second);
    requestUrlQuery.addQueryItem("format", nx::toString(localPeer().dataFormat));

    requestUrl.setQuery(requestUrlQuery.toString());
    if (requestUrl.password().isEmpty())
        fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey);

    m_httpClient->doGet(
        requestUrl,
        std::bind(&ConnectionBase::onHttpClientDone, this));
}

void ConnectionBase::startReading()
{
    m_startedClassId = typeid(*this).hash_code();

    NX_VERBOSE(this, "Connection Starting reading, state [%1]", state());
    using namespace std::placeholders;
    m_p2pTransport->readSomeAsync(
        &m_readBuffer,
        std::bind(&ConnectionBase::onNewMessageRead, this, _1, _2));
}

ConnectionBase::State ConnectionBase::state() const
{
    return m_state;
}

void ConnectionBase::setState(State state)
{
    if (state == m_state)
        return;

    if (state != State::Error && m_state >= State::Error)
    {
        if (NX_ASSERT(state > State::Error,
            "State %1 is final and should not be changed to %2",
            toString(m_state), toString(state)))
        {
            NX_VERBOSE(
                this, "Ignore state change: [%1] -> [%2]", toString(m_state), toString(state));
        }
        return;
    }

    NX_VERBOSE(this,
        "Connection State change: [%1] -> [%2]",
        toString(m_state), toString(state));
    m_state = state;
    emit stateChanged(weakPointer(), state);
}

void ConnectionBase::sendMessage(MessageType messageType, const nx::Buffer& data)
{
    if (remotePeer().isClient())
        NX_ASSERT(messageType == MessageType::pushTransactionData);

    if (remotePeer().isCloudServer())
    {
        NX_ASSERT(messageType == MessageType::pushTransactionData
            || messageType == MessageType::subscribeAll);
    }

    nx::Buffer buffer;
    buffer.reserve(data.size() + 1);
    buffer.append((char) messageType);
    buffer.append(data);
    sendMessage(buffer);
}

void ConnectionBase::sendMessage(MessageType messageType, const QByteArray& data)
{
    sendMessage(messageType, nx::Buffer(data));
}

MessageType ConnectionBase::getMessageType(const nx::Buffer& buffer, bool isClient) const
{
    if (isClient)
        return MessageType::pushTransactionData;

    auto messageType = buffer.at(kMessageOffset);
    return messageType < (qint8) MessageType::counter
        ? (MessageType) messageType
        : MessageType::unknown;
}

void ConnectionBase::setMaxSendBufferSize(size_t value)
{
    m_timer.post(
        [this, value]()
        {
            m_extraBufferSize = m_dataToSend.dataSize();
            m_maxBufferSize = value;
        });
}

void ConnectionBase::sendMessage(const nx::Buffer& data)
{
    NX_ASSERT(!data.empty());

    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this) && qnStaticCommon)
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(localPeer().id);
        auto remotePeerName = qnStaticCommon->moduleDisplayName(remotePeer().id);

        MessageType messageType = (MessageType)data[0];
        if (messageType != MessageType::pushTransactionData &&
            messageType != MessageType::pushTransactionList)
        {
            NX_VERBOSE(this, "Send message: %1 ---> %2. Type: %3. Size=%4",
                localPeerName, remotePeerName, messageType, data.size());
        }
    }

    m_timer.post(
        [this, data]()
        {
#ifdef CHECK_SEQUENCE
            QByteArray dataWithSequence;
            dataWithSequence.resize(8);
            quint32* dataPtr = (quint32*) dataWithSequence.data();
            dataPtr[0] = htonl(m_sendSequence++);
            dataPtr[1] = htonl(data.size());
            dataWithSequence.append(data);
            m_dataToSend.push_back(dataWithSequence);
#else
            m_dataToSend.push_back(data);
#endif
            if (m_maxBufferSize > 0 && m_dataToSend.dataSize() > m_maxBufferSize + m_extraBufferSize)
            {
                NX_WARNING(this,
                    "p2p send queue overflow for peer %1, queue size: %2. Close connection.",
                    remotePeer().id, m_dataToSend.dataSize());
                setState(State::Error);
                return;
            }

            if (m_dataToSend.size() == 1)
            {
                auto messageType = getMessageType(m_dataToSend.front(), remotePeer().isClient());
                m_sendCounters[(quint8)messageType] += m_dataToSend.front().size();

                using namespace std::placeholders;
                m_p2pTransport->sendAsync(
                    &m_dataToSend.front(),
                    std::bind(&ConnectionBase::onMessageSent, this, _1, _2));
            }
        }
    );
}

void ConnectionBase::sendMessage(const QByteArray& data)
{
    sendMessage(nx::Buffer(data));
}

void ConnectionBase::onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent)
{
    if (errorCode != SystemError::noError ||
        bytesSent == 0)
    {
        NX_DEBUG(
            this, "onMessageSent: Connection closed. Error: %1, bytesSent: %2",
            errorCode, bytesSent);
        setState(State::Error);
        return;
    }

    using namespace std::placeholders;
    m_extraBufferSize = std::max(int64_t(0), m_extraBufferSize - int64_t(m_dataToSend.front().size()));
    m_dataToSend.pop_front();
    if (!m_dataToSend.empty())
    {
        quint8 messageType = (quint8) getMessageType(m_dataToSend.front(), remotePeer().isClient());
        m_sendCounters[messageType] += m_dataToSend.front().size();

        m_p2pTransport->sendAsync(
            &m_dataToSend.front(),
            std::bind(&ConnectionBase::onMessageSent, this, _1, _2));
    }
    else
    {
        emit allDataSent(weakPointer());
    }
}

void ConnectionBase::transactionSkipped()
{
    if (m_dataToSend.empty())
        emit allDataSent(weakPointer());
}

void ConnectionBase::onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead)
{
    if (bytesRead == 0)
    {
        NX_DEBUG( this, "onNewMessageRead: Connection closed by remote peer");
        setState(State::Error);
        return; //< connection closed
    }

    if (errorCode != SystemError::noError || !handleMessage(m_readBuffer))
    {
        NX_DEBUG( this, "onNewMessageRead: Connection closed with error: %1", errorCode);
        setState(State::Error);
        return;
    }

    using namespace std::placeholders;
    m_readBuffer.resize(0);
    m_p2pTransport->readSomeAsync(
        &m_readBuffer,
        std::bind(&ConnectionBase::onNewMessageRead, this, _1, _2));
}

int ConnectionBase::messageHeaderSize(bool isClient) const
{
    // kMessageOffset is an addition optional header for debug purpose. Usual header has 1 byte for server.
    return isClient ? 0 : kMessageOffset + 1;
}

bool ConnectionBase::handleMessage(const nx::Buffer& message)
{
    NX_ASSERT(!message.empty());
#ifdef CHECK_SEQUENCE
    quint32* dataPtr = (quint32*) message.data();
    quint32 sequence = ntohl(dataPtr[0]);
    quint32 dataSize = ntohl(dataPtr[1]);

    NX_CRITICAL(sequence == m_lastReceivedSequence);
    m_lastReceivedSequence++;
    NX_CRITICAL(dataSize == message.size() - kMessageOffset);
#endif

    const bool isClient = localPeer().isClient();
    MessageType messageType = getMessageType(message, isClient);
    emit gotMessage(weakPointer(), messageType, message.substr(messageHeaderSize(isClient)));

    return true;
}

std::multimap<QString, QString> ConnectionBase::httpQueryParams() const
{
    return m_remoteQueryParams;
}

QObject* ConnectionBase::opaqueObject()
{
    return m_opaqueObject.get();
}

void ConnectionBase::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    m_timer.bindToAioThread(aioThread);
    if (m_httpClient)
        m_httpClient->bindToAioThread(aioThread);

    if (m_p2pTransport)
        m_p2pTransport->bindToAioThread(aioThread);
}

} // namespace p2p
} // namespace nx
