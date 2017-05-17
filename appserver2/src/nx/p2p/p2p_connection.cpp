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
#include <transaction/transaction_transport_base.h>

namespace nx {
namespace p2p {

SendCounters Connection::m_sendCounters = {};

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
        fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey);
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
    const nx::network::WebSocketPtr& webSocket,
    std::unique_ptr<QObject> opaqueObject)
:
    QnCommonModuleAware(commonModule),
    m_remotePeer(remotePeer),
    m_localPeer(localPeer),
    m_webSocket(webSocket),
    m_state(State::Connected),
    m_direction(Direction::incoming),
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

void Connection::fillAuthInfo(nx_http::AsyncClient* httpClient, bool authByKey)
{
    if (!commonModule()->videowallGuid().isNull())
    {
        m_httpClient->addAdditionalHeader(
            Qn::VIDEOWALL_GUID_HEADER_NAME,
            commonModule()->videowallGuid().toString().toUtf8());
        return;
    }

    const auto& resPool = commonModule()->resourcePool();
    QnMediaServerResourcePtr ownServer =
        resPool->getResourceById<QnMediaServerResource>(localPeer().id);
    if (ownServer && authByKey)
    {
        m_httpClient->setUserName(ownServer->getId().toString().toLower());
        m_httpClient->setUserPassword(ownServer->getAuthKey());
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

void Connection::cancelConnecting(const QString& reason)
{
    NX_LOG(QnLog::P2P_TRAN_LOG,
        lit("Connection to peer %1 canceled from state %2. Reason: %3")
        .arg(m_remotePeer.id.toString())
        .arg(toString(state()))
        .arg(reason),
        cl_logDEBUG2);
    setState(State::Error);
}

void Connection::onHttpClientDone()
{
    nx_http::AsyncClient::State state = m_httpClient->state();
    if (state == nx_http::AsyncClient::sFailed)
    {
        cancelConnecting(lm("Http request failed"));
        return;
    }

    const int statusCode = m_httpClient->response()->statusLine.statusCode;

    NX_LOG(QnLog::P2P_TRAN_LOG, lit("%1. statusCode = %2")
        .arg(Q_FUNC_INFO).arg(statusCode), cl_logDEBUG2);

    if (statusCode == nx_http::StatusCode::unauthorized)
    {
        // try next credential source
        m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
        if (m_credentialsSource < CredentialsSource::none)
        {
            using namespace std::placeholders;
            fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey);
            m_httpClient->doPost(
                m_remotePeerUrl,
                std::bind(&Connection::onHttpClientDone, this));
        }
        else
        {
            cancelConnecting(lm("Unauthorized"));
        }
        return;
    }
    else if (!nx_http::StatusCode::isSuccessCode(statusCode)) //< Checking that statusCode is 2xx.
    {
        cancelConnecting(lm("Not success HTTP status code %1").arg(statusCode));
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
            cancelConnecting(lm("tryAcquireConnecting failed"));
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
        cancelConnecting(lm("Remote peer Id is null"));
        return;
    }

    m_remotePeer = remotePeer;

    NX_ASSERT(!m_remotePeer.instanceId.isNull());
    if (m_remotePeer.id == ec2::kCloudPeerId)
        m_remotePeer.peerType = Qn::PT_CloudServer;

    if (!m_connectionLockGuard->tryAcquireConnected())
    {
        cancelConnecting(lm("tryAcquireConnected failed"));
        return;
    }

    using namespace nx::network;
    auto error = websocket::validateResponse(m_httpClient->request(), *m_httpClient->response());
    if (error != websocket::Error::noError)
    {
        NX_LOG(QnLog::P2P_TRAN_LOG,
            lm("Can't establish WEB socket connection. Validation failed. Error: %1").arg((int)error), cl_logERROR);
        setState(State::Error);
        m_httpClient.reset();
        return;
    }

    auto socket = m_httpClient->takeSocket();
    auto keepAliveTimeout = commonModule()->globalSettings()->connectionKeepAliveTimeout() * 1000;
    socket->setRecvTimeout(std::chrono::milliseconds(keepAliveTimeout * 2).count());
    socket->setSendTimeout(std::chrono::milliseconds(keepAliveTimeout * 2).count());
    socket->setNonBlockingMode(true);
    socket->setNoDelay(true);

    using namespace nx::network;
    m_webSocket.reset(new websocket::WebSocket(std::move(socket)));
    m_httpClient.reset();
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
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
        auto remotePeerName = qnStaticCommon->moduleDisplayName(remotePeer().id);

        MessageType messageType = (MessageType)data[0];
        if (messageType != MessageType::pushTransactionData &&
            messageType != MessageType::pushTransactionList)
        {
            NX_LOG(QnLog::P2P_TRAN_LOG,
                lit("Send message:\t %1 ---> %2. Type: %3. Size=%4")
                .arg(localPeerName)
                .arg(remotePeerName)
                .arg(toString(messageType))
                .arg(data.size()),
                cl_logDEBUG1);
        }
    }

    m_timer.post(
        [this, data]()
        {
            m_dataToSend.push_back(data);
            if (m_dataToSend.size() == 1)
            {
                quint8 messageType = (quint8)m_dataToSend.front().at(0);
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
        quint8 messageType = (quint8)m_dataToSend.front().at(0);
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
    MessageType messageType = (MessageType) message[0];
    NX_ASSERT(!m_remotePeer.persistentId.isNull());
    emit gotMessage(weakPointer(), messageType, message.mid(1));
    return true;
}

QObject* Connection::opaqueObject()
{
    return m_opaqueObject.get();
}

} // namespace p2p
} // namespace nx
