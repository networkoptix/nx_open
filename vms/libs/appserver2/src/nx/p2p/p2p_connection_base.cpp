#include "p2p_connection_base.h"
#include "p2p_serialization.h"

#include <QtCore/QUrlQuery>

#include <nx/utils/log/log_message.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/network/http/custom_headers.h>
#include <nx/fusion/serialization/lexical.h>
#include <common/static_common_module.h>
#include <nx/network/http/buffer_source.h>
#include <transaction/transaction_message_bus_base.h>
#include <nx/p2p/transport/p2p_http_client_transport.h>
#include <nx/p2p/transport/p2p_http_server_transport.h>
#include <nx/p2p/transport/p2p_websocket_transport.h>
#include <nx/p2p/transport/p2p_http_client_transport.h>

// For debug purpose only
//#define CHECK_SEQUENCE

namespace nx {
namespace p2p {

SendCounters ConnectionBase::m_sendCounters = {};

const QString ConnectionBase::kWebsocketUrlPath(lit("/ec2/transactionBus/websocket"));
const QString ConnectionBase::kHttpUrlPath(lit("/ec2/transactionBus/http"));


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
    default:
        NX_ASSERT(false, "Unknown enum value");
        return "Unknown";
    }
}

ConnectionBase::ConnectionBase(
    const QnUuid& remoteId,
    const vms::api::PeerDataEx& localPeer,
    const nx::utils::Url& _remotePeerUrl,
    const std::chrono::seconds& keepAliveTimeout,
    std::unique_ptr<QObject> opaqueObject,
    std::unique_ptr<ConnectionLockGuard> connectionLockGuard)
:
    m_direction(Direction::outgoing),
    m_httpClient(new nx::network::http::AsyncClient()),
    m_localPeer(localPeer),
    m_keepAliveTimeout(keepAliveTimeout),
    m_opaqueObject(std::move(opaqueObject)),
    m_connectionLockGuard(std::move(connectionLockGuard))
{
    m_remotePeerUrl = _remotePeerUrl;
    m_remotePeer.id = remoteId;
    NX_ASSERT(m_localPeer.id != m_remotePeer.id);
    m_httpClient->setSendTimeout(keepAliveTimeout);
    m_httpClient->setResponseReadTimeout(keepAliveTimeout);

    bindToAioThread(m_timer.getAioThread());
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
    m_connectionLockGuard(std::move(connectionLockGuard))
{
    NX_ASSERT(m_localPeer.id != m_remotePeer.id);

    bindToAioThread(m_p2pTransport->getAioThread());

    const auto& queryItems = requestUrlQuery.queryItems();
    std::transform(
        queryItems.begin(), queryItems.end(),
        std::inserter(m_remoteQueryParams, m_remoteQueryParams.end()),
        [](const QPair<QString, QString>& item)
            { return std::make_pair(item.first, item.second); });
}

void ConnectionBase::gotPostConnection(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    nx::Buffer requestBody)
{
    m_timer.post(
        [this, socket = std::move(socket), requestBody = std::move(requestBody)]() mutable
        {
            using namespace nx::network;
            if(auto httpTransport = dynamic_cast<P2PHttpServerTransport*>(m_p2pTransport.get()))
            {
                httpTransport->gotPostConnection(std::move(socket), std::move(requestBody));
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
        m_startedClassId = 0;
        m_timer.executeInAioThreadSync([this]() { stopWhileInAioThread(); });
    }
}

ConnectionBase::~ConnectionBase()
{
    pleaseStopSync();
}

nx::utils::Url ConnectionBase::remoteAddr() const
{
    if (m_direction == Direction::outgoing)
        return m_remotePeerUrl;
    if (m_p2pTransport)
    {
        auto address = m_p2pTransport->getForeignAddress();
        return nx::utils::Url(lit("http://%1:%2")
            .arg(address.address.toString())
            .arg(address.port));
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
    setState(newState);
}

QString ConnectionBase::idForToStringFromPtr() const
{
    return remotePeer().id.toString();
}

void ConnectionBase::onHttpClientDone()
{
    nx::network::http::AsyncClient::State state = m_httpClient->state();

    if (state == nx::network::http::AsyncClient::State::sFailed)
    {
        cancelConnecting(State::Error, lm("Http request failed %1").arg(m_httpClient->lastSysErrorCode()));
        return;
    }

    const int statusCode = m_httpClient->response()->statusLine.statusCode;

    NX_VERBOSE(this, lit("%1. statusCode = %2").arg(Q_FUNC_INFO).arg(statusCode));

    if (statusCode == nx::network::http::StatusCode::unauthorized)
    {
        // try next credential source
        m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
        using namespace std::placeholders;
        if (m_credentialsSource < CredentialsSource::none
            && fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey))
        {
            m_httpClient->doGet(
                m_httpClient->url(),
                std::bind(&ConnectionBase::onHttpClientDone, this));
        }
        else
        {
            cancelConnecting(State::Unauthorized, lm("Unauthorized"));
        }
        return;
    }

    const auto& headers = m_httpClient->response()->headers;
    if (m_connectionLockGuard && headers.find(Qn::EC2_CONNECT_STAGE_1) != headers.end())
    {
        // Addition stage for server to server connect. It prevents to open two (incoming and outgoing) connections at once.
        if (!nx::network::http::StatusCode::isSuccessCode(statusCode)) //< Checking that statusCode is 2xx.
        {
            cancelConnecting(State::Error, lm("Not a successful HTTP status code %1").arg(statusCode));
            return;
        }

        if (m_connectionLockGuard->tryAcquireConnecting())
            m_httpClient->doGet(
                m_httpClient->url(),
                std::bind(&ConnectionBase::onHttpClientDone, this));
        else
            cancelConnecting(State::Error, lm("tryAcquireConnecting failed"));
        return;
    }

    vms::api::PeerDataEx remotePeer = deserializePeerData(headers, m_localPeer.dataFormat);

    if (remotePeer.id.isNull())
    {
        cancelConnecting(State::Error, lm("Remote peer Id is null"));
        return;
    }
    else if (remotePeer.id != m_remotePeer.id)
    {
        cancelConnecting(
            State::Error,
            lm("Remote peer id %1 is not match expected peer id %2")
            .arg(remotePeer.id.toString())
            .arg(m_remotePeer.id.toString()));
        return;
    }
    else if (!validateRemotePeerData(remotePeer))
    {
        cancelConnecting(
            State::Error,
            lm("Remote peer id %1 has inappropriate data to make connection.")
            .arg(remotePeer.id.toString()));
        return;
    }

    m_remotePeer = remotePeer;

    NX_ASSERT(!m_remotePeer.instanceId.isNull());

    if (m_connectionLockGuard && !m_connectionLockGuard->tryAcquireConnected())
    {
        cancelConnecting(State::Error, lm("tryAcquireConnected failed"));
        return;
    }

    using namespace nx::network;
    using namespace nx::vms::api;

    auto error = websocket::validateResponse(m_httpClient->request(), *m_httpClient->response());
    auto useWebsocketMode = error == websocket::Error::noError;
    if (!useWebsocketMode)
    {
        NX_WARNING(this,
            lm("Can't establish WEB socket connection. Validation failed. Error: %1. Switch to the HTTP mode").arg((int)error));
    }

    //saving credentials we used to authorize request
    if (m_httpClient->request().headers.find(nx::network::http::header::Authorization::NAME) !=
        m_httpClient->request().headers.end())
    {
        QnMutexLocker lock(&m_mutex);
        m_httpAuthCacheItem = m_httpClient->authCacheItem();
    }

    using namespace nx::network;
    websocket::FrameType frameType = remotePeer.dataFormat == Qn::JsonFormat
        ? websocket::FrameType::text
        : websocket::FrameType::binary;
    if (useWebsocketMode)
    {
        auto socket = m_httpClient->takeSocket();
        socket->setNonBlockingMode(true);
        m_p2pTransport.reset(new P2PWebsocketTransport(std::move(socket), frameType));
    }
    else
    {
        auto url = m_httpClient->url();
        auto urlPath = url.path().replace(kWebsocketUrlPath, kHttpUrlPath);
        url.setPath(urlPath);

        m_p2pTransport.reset(new P2PHttpClientTransport(
            std::move(m_httpClient),
            m_connectionGuid,
            network::websocket::FrameType::binary,
            url));
    }

    m_p2pTransport->start();
    m_httpClient.reset();
    setState(State::Connected);
}

void ConnectionBase::startConnection()
{
     m_startedClassId = typeid(*this).hash_code();

    auto headers = m_additionalRequestHeaders;
    nx::network::websocket::addClientHeaders(&headers, kP2pProtoName);
    m_connectionGuid = QnUuid::createUuid().toByteArray();
    headers.emplace(Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_connectionGuid);
    m_httpClient->addRequestHeaders(headers);

    auto requestUrl = m_remotePeerUrl;
    QUrlQuery requestUrlQuery(requestUrl.query());
    for (const auto& param: m_requestQueryParams)
        requestUrlQuery.addQueryItem(param.first, param.second);
    requestUrlQuery.addQueryItem("format", QnLexical::serialized(localPeer().dataFormat));

    requestUrl.setQuery(requestUrlQuery.toString());

    m_httpClient->bindToAioThread(m_timer.getAioThread());

    if (requestUrl.userName().isEmpty())
    {
        fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey);
    }
    else
    {
        m_credentialsSource = CredentialsSource::userNameAndPassword;
        m_httpClient->setUserName(requestUrl.userName());
        m_httpClient->setUserPassword(requestUrl.password());
    }

    m_httpClient->doGet(
        requestUrl,
        std::bind(&ConnectionBase::onHttpClientDone, this));
}

void ConnectionBase::startReading()
{
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

    NX_ASSERT(m_state != State::Error, "State 'Error' is final and should not be changed");
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

MessageType ConnectionBase::getMessageType(const nx::Buffer& buffer, bool isClient) const
{
    if (isClient)
        return MessageType::pushTransactionData;

    auto messageType = buffer.at(kMessageOffset);
    return messageType < (qint8) MessageType::counter
        ? (MessageType) messageType
        : MessageType::unknown;
}

void ConnectionBase::sendMessage(const nx::Buffer& data)
{
    NX_ASSERT(!data.isEmpty());

    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this) && qnStaticCommon)
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(localPeer().id);
        auto remotePeerName = qnStaticCommon->moduleDisplayName(remotePeer().id);

        MessageType messageType = (MessageType)data[0];
        if (messageType != MessageType::pushTransactionData &&
            messageType != MessageType::pushTransactionList)
        {
            NX_VERBOSE(
                this,
                lit("Send message: %1 ---> %2. Type: %3. Size=%4")
                .arg(localPeerName)
                .arg(remotePeerName)
                .arg(toString(messageType))
                .arg(data.size()));
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

            if (m_dataToSend.size() == 1)
            {
                auto messageType = getMessageType(m_dataToSend.front(), remotePeer().isClient());
                m_sendCounters[(quint8)messageType] += m_dataToSend.front().size();

                using namespace std::placeholders;
                m_p2pTransport->sendAsync(
                    m_dataToSend.front(),
                    std::bind(&ConnectionBase::onMessageSent, this, _1, _2));
            }
        }
    );
}

void ConnectionBase::onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent)
{
    if (errorCode != SystemError::noError ||
        bytesSent == 0)
    {
        setState(State::Error);
        return;
    }
    using namespace std::placeholders;
    m_dataToSend.pop_front();
    if (!m_dataToSend.empty())
    {
        quint8 messageType = (quint8) getMessageType(m_dataToSend.front(), remotePeer().isClient());
        m_sendCounters[messageType] += m_dataToSend.front().size();

        m_p2pTransport->sendAsync(
            m_dataToSend.front(),
            std::bind(&ConnectionBase::onMessageSent, this, _1, _2));
    }
    else
    {
        emit allDataSent(weakPointer());
    }
}

void ConnectionBase::onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead)
{
    if (bytesRead == 0)
    {
        setState(State::Error);
        return; //< connection closed
    }

    if (errorCode != SystemError::noError ||
        !handleMessage(m_readBuffer))
    {
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
    NX_ASSERT(!message.isEmpty());
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
    emit gotMessage(weakPointer(), messageType, message.mid(messageHeaderSize(isClient)));

    return true;
}

nx::network::http::AuthInfoCache::AuthorizationCacheItem ConnectionBase::authData() const
{
    QnMutexLocker lock(&m_mutex);
    return m_httpAuthCacheItem;
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
