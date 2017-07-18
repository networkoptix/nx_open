#include "p2p_connection.h"
#include <nx/utils/log/log_message.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/network/http/custom_headers.h>
#include <nx/fusion/serialization/lexical.h>
#include <api/global_settings.h>
#include <common/static_common_module.h>
#include <nx/network/http/buffer_source.h>
#include <transaction/transaction_message_bus_base.h>
#include "p2p_serialization.h"

// For debug purpose only
//#define CHECK_SEQUENCE

namespace nx {
namespace p2p {

SendCounters Connection::m_sendCounters = {};

#ifdef CHECK_SEQUENCE
const int kMessageOffset = 8;
#else
const int kMessageOffset = 0;
#endif


QString toString(Connection::State value)
{
    switch (value)
    {
    case Connection::State::Connecting:
        return lm("Connecting");
    case Connection::State::Connected:
        return lm("Connected");
    case Connection::State::Error:
        return lm("Error");
    default:
        return lm("Unknown");
    }
}

Connection::Connection(
    QnCommonModule* commonModule,
    const QnUuid& remoteId,
    const ApiPeerDataEx& localPeer,
    ConnectionLockGuard connectionLockGuard,
    const QUrl& _remotePeerUrl,
    std::unique_ptr<QObject> opaqueObject)
:
    QnCommonModuleAware(commonModule),
    m_httpClient(new nx_http::AsyncClient()),
    m_localPeer(localPeer),
    m_direction(Direction::outgoing),
    m_opaqueObject(std::move(opaqueObject))
{
    m_connectionLockGuard = std::make_unique<ConnectionLockGuard>(
        std::move(connectionLockGuard));

    m_remotePeerUrl = _remotePeerUrl;
    m_remotePeer.id = remoteId;
    NX_ASSERT(m_localPeer.id != m_remotePeer.id);

    if (m_remotePeerUrl.userName().isEmpty())
    {
        fillAuthInfo(m_credentialsSource == CredentialsSource::serverKey);
    }
    else
    {
        m_credentialsSource = CredentialsSource::remoteUrl;
        m_httpClient->setUserName(m_remotePeerUrl.userName());
        m_httpClient->setUserPassword(m_remotePeerUrl.password());
    }

    nx_http::Request request;
    nx::network::websocket::addClientHeaders(&request, kP2pProtoName);
    request.headers.emplace(Qn::EC2_PEER_DATA, QnUbjson::serialized(m_localPeer).toBase64());
    m_httpClient->addRequestHeaders(request.headers);
    m_httpClient->bindToAioThread(m_timer.getAioThread());
}

Connection::Connection(
    QnCommonModule* commonModule,
    const ApiPeerDataEx& remotePeer,
    const ApiPeerDataEx& localPeer,
    ConnectionLockGuard connectionLockGuard,
    nx::network::WebSocketPtr webSocket,
    const Qn::UserAccessData& userAccessData,
    std::unique_ptr<QObject> opaqueObject)
:
    QnCommonModuleAware(commonModule),
    m_remotePeer(remotePeer),
    m_localPeer(localPeer),
    m_webSocket(std::move(webSocket)),
    m_state(State::Connected),
    m_direction(Direction::incoming),
    m_userAccessData(userAccessData),
    m_opaqueObject(std::move(opaqueObject))
{
    m_connectionLockGuard = std::make_unique<ConnectionLockGuard>(
        std::move(connectionLockGuard));

    NX_ASSERT(m_localPeer.id != m_remotePeer.id);
    m_timer.bindToAioThread(m_webSocket->getAioThread());
}

Connection::~Connection()
{
    std::promise<void> waitToStop;
    m_timer.pleaseStop(
        [&]()
        {
            if (m_webSocket)
                m_webSocket->pleaseStopSync();
            if (m_httpClient)
                m_httpClient->pleaseStopSync();
            waitToStop.set_value();
        }
    );
    waitToStop.get_future().wait();
}

QUrl Connection::remoteAddr() const
{
    if (m_direction == Direction::outgoing)
        return m_remotePeerUrl;
    if (m_webSocket)
    {
        auto address = m_webSocket->socket()->getForeignAddress();
        return QUrl(lit("http://%1:%2")
            .arg(address.address.toString())
            .arg(address.port));
    }
    return QUrl();
}

void Connection::fillAuthInfo(bool authByKey)
{
    if (!commonModule()->videowallGuid().isNull())
    {
        m_httpClient->addAdditionalHeader(
            Qn::VIDEOWALL_GUID_HEADER_NAME,
            commonModule()->videowallGuid().toString().toUtf8());
        return;
    }

    const auto& resPool = commonModule()->resourcePool();
    QnMediaServerResourcePtr server =
        resPool->getResourceById<QnMediaServerResource>(remotePeer().id);
    if (!server)
        server = resPool->getResourceById<QnMediaServerResource>(localPeer().id);
    if (server && authByKey)
    {
        m_httpClient->setUserName(server->getId().toString().toLower());
        m_httpClient->setUserPassword(server->getAuthKey());
    }
    else
    {
        QUrl url;
        if (const auto& connection = commonModule()->ec2Connection())
            url = connection->connectionInfo().ecUrl;
        m_httpClient->setUserName(url.userName().toLower());
        if (ApiPeerData::isServer(localPeer().peerType))
        {
            // try auth by admin user if allowed
            QnUserResourcePtr adminUser = resPool->getAdministrator();
            if (adminUser)
            {
                m_httpClient->setUserPassword(adminUser->getDigest());
                m_httpClient->setAuthType(nx_http::AsyncClient::authDigestWithPasswordHash);
            }
        }
        else
        {
            m_httpClient->setUserPassword(url.password());
        }
    }
}

void Connection::cancelConnecting(State newState, const QString& reason)
{
    NX_VERBOSE(
        this,
        lit("Connection to peer %1 canceled from state %2. Reason: %3")
        .arg(m_remotePeer.id.toString())
        .arg(toString(state()))
        .arg(reason));
    setState(newState);
}

void Connection::onHttpClientDone()
{
    nx_http::AsyncClient::State state = m_httpClient->state();
    if (state == nx_http::AsyncClient::sFailed)
    {
        cancelConnecting(State::Error, lm("Http request failed"));
        return;
    }

    const int statusCode = m_httpClient->response()->statusLine.statusCode;

    NX_VERBOSE(this, lit("%1. statusCode = %2").arg(Q_FUNC_INFO).arg(statusCode));

    if (statusCode == nx_http::StatusCode::unauthorized)
    {
        // try next credential source
        m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
        if (m_credentialsSource < CredentialsSource::none)
        {
            using namespace std::placeholders;
            fillAuthInfo(m_credentialsSource == CredentialsSource::serverKey);
            m_httpClient->doGet(
                m_remotePeerUrl,
                std::bind(&Connection::onHttpClientDone, this));
        }
        else
        {
            cancelConnecting(State::Unauthorized, lm("Unauthorized"));
        }
        return;
    }
    else if (!nx_http::StatusCode::isSuccessCode(statusCode)) //< Checking that statusCode is 2xx.
    {
        cancelConnecting(State::Error, lm("Not success HTTP status code %1").arg(statusCode));
        return;
    }

    const auto& headers = m_httpClient->response()->headers;
    if (headers.find(Qn::EC2_CONNECT_STAGE_1) != headers.end())
    {
        // Addition stage for server to server connect. It prevents to open two (incoming and outgoing) connections at once.
        if (m_connectionLockGuard->tryAcquireConnecting())
            m_httpClient->doGet(
                m_remotePeerUrl,
                std::bind(&Connection::onHttpClientDone, this));
        else
            cancelConnecting(State::Error, lm("tryAcquireConnecting failed"));
        return;
    }

    ApiPeerDataEx remotePeer;
    QByteArray serializedPeerData = nx_http::getHeaderValue(headers, Qn::EC2_PEER_DATA);
    serializedPeerData = QByteArray::fromBase64(serializedPeerData);

    bool success = false;
    if (m_localPeer.dataFormat == Qn::JsonFormat)
        remotePeer = QJson::deserialized(serializedPeerData, ApiPeerDataEx(), &success);
    else if (m_localPeer.dataFormat == Qn::UbjsonFormat)
        remotePeer = QnUbjson::deserialized(serializedPeerData, ApiPeerDataEx(), &success);

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
    m_remotePeer = remotePeer;

    NX_ASSERT(!m_remotePeer.instanceId.isNull());
    if (m_remotePeer.id == ::ec2::kCloudPeerId)
        m_remotePeer.peerType = Qn::PT_CloudServer;

    if (!m_connectionLockGuard->tryAcquireConnected())
    {
        cancelConnecting(State::Error, lm("tryAcquireConnected failed"));
        return;
    }

    using namespace nx::network;
    auto error = websocket::validateResponse(m_httpClient->request(), *m_httpClient->response());
    if (error != websocket::Error::noError)
    {
        NX_ERROR(this,
            lm("Can't establish WEB socket connection. Validation failed. Error: %1").arg((int)error));
        setState(State::Error);
        m_httpClient.reset();
        return;
    }

    //saving credentials we used to authorize request
    if (m_httpClient->request().headers.find(nx_http::header::Authorization::NAME) !=
        m_httpClient->request().headers.end())
    {
        QnMutexLocker lock(&m_mutex);
        m_httpAuthCacheItem = m_httpClient->authCacheItem();
    }

    auto socket = m_httpClient->takeSocket();
    socket->setNonBlockingMode(true);

    using namespace nx::network;
    m_webSocket.reset(new websocket::WebSocket(std::move(socket)));
    m_httpClient.reset();
    auto keepAliveTimeout = commonModule()->globalSettings()->connectionKeepAliveTimeout();
    m_webSocket->setAliveTimeoutEx(keepAliveTimeout, 2);

    setState(State::Connected);
}

void Connection::startConnection()
{
    m_httpClient->doGet(
        m_remotePeerUrl,
        std::bind(&Connection::onHttpClientDone, this));
}

void Connection::startReading()
{
    using namespace std::placeholders;
    m_webSocket->readSomeAsync(
        &m_readBuffer,
        std::bind(&Connection::onNewMessageRead, this, _1, _2));
}

Connection::State Connection::state() const
{
    return m_state;
}

void Connection::setState(State state)
{
    if (state != m_state)
    {
        m_state = state;
        emit stateChanged(weakPointer(), state);
    }
}

void Connection::sendMessage(MessageType messageType, const nx::Buffer& data)
{
    nx::Buffer buffer;
    buffer.reserve(data.size() + 1);
    buffer.append((char) messageType);
    buffer.append(data);
    sendMessage(buffer);
}

void Connection::sendMessage(const nx::Buffer& data)
{
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, this))
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
        auto remotePeerName = qnStaticCommon->moduleDisplayName(remotePeer().id);

        MessageType messageType = (MessageType)data[0];
        if (messageType != MessageType::pushTransactionData &&
            messageType != MessageType::pushTransactionList)
        {
            NX_DEBUG(
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
                quint8 messageType = (quint8)m_dataToSend.front().at(kMessageOffset);
                m_sendCounters[messageType] += m_dataToSend.front().size();

                using namespace std::placeholders;
                m_webSocket->sendAsync(
                    m_dataToSend.front(),
                    std::bind(&Connection::onMessageSent, this, _1, _2));
            }
        }
    );
}

void Connection::onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent)
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
        quint8 messageType = (quint8)m_dataToSend.front().at(kMessageOffset);
        m_sendCounters[messageType] += m_dataToSend.front().size();

        m_webSocket->sendAsync(
            m_dataToSend.front(),
            std::bind(&Connection::onMessageSent, this, _1, _2));
    }
    else
    {
        emit allDataSent(weakPointer());
    }
}

void Connection::onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead)
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
    m_webSocket->readSomeAsync(
        &m_readBuffer,
        std::bind(&Connection::onNewMessageRead, this, _1, _2));
}

bool Connection::handleMessage(const nx::Buffer& message)
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

    MessageType messageType = (MessageType)message[kMessageOffset];
    emit gotMessage(weakPointer(), messageType, message.mid(kMessageOffset + 1));

    return true;
}

nx_http::AuthInfoCache::AuthorizationCacheItem Connection::authData() const
{
    QnMutexLocker lock(&m_mutex);
    return m_httpAuthCacheItem;
}

QObject* Connection::opaqueObject()
{
    return m_opaqueObject.get();
}

} // namespace p2p
} // namespace nx
