#include "p2p_connection.h"
#include <nx/utils/log/log_message.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <http/custom_headers.h>
#include <nx/fusion/serialization/lexical.h>
#include <api/global_settings.h>
#include <common/static_common_module.h>
#include <nx/network/http/buffer_source.h>
#include <transaction/transaction_message_bus_base.h>
#include "p2p_serialization.h"
#include <transaction/transaction_transport_base.h>

namespace nx {
namespace p2p {

SendCounters P2pConnection::m_sendCounters = {};

const char* toString(P2pConnection::State value)
{
    switch (value)
    {
    case P2pConnection::State::Connecting:
        return "Connecting";
    case P2pConnection::State::Connected:
        return "Connected";
    case P2pConnection::State::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

P2pConnection::P2pConnection(
    QnCommonModule* commonModule,
    const QnUuid& remoteId,
    const ApiPeerDataEx& localPeer,
    ConnectionLockGuard connectionLockGuard,
    const QUrl& _remotePeerUrl)
:
    QnCommonModuleAware(commonModule),
    m_httpClient(new nx_http::AsyncClient()),
    m_localPeer(localPeer),
    m_direction(Direction::outgoing)
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

P2pConnection::P2pConnection(
    QnCommonModule* commonModule,
    const ApiPeerDataEx& remotePeer,
    const ApiPeerDataEx& localPeer,
    ConnectionLockGuard connectionLockGuard,
    const nx::network::WebSocketPtr& webSocket)
:
    QnCommonModuleAware(commonModule),
    m_remotePeer(remotePeer),
    m_localPeer(localPeer),
    m_webSocket(webSocket),
    m_state(State::Connected),
    m_direction(Direction::incoming)
{
    m_connectionLockGuard = std::make_unique<ConnectionLockGuard>(
        std::move(connectionLockGuard));

    NX_ASSERT(m_localPeer.id != m_remotePeer.id);
    m_timer.bindToAioThread(m_webSocket->getAioThread());
}

P2pConnection::~P2pConnection()
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

void P2pConnection::fillAuthInfo(nx_http::AsyncClient* httpClient, bool authByKey)
{
    if (!commonModule()->videowallGuid().isNull())
    {
        m_httpClient->addAdditionalHeader(
            "X-NetworkOptix-VideoWall",
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

void P2pConnection::cancelConnecting()
{
    NX_LOG(QnLog::P2P_TRAN_LOG,
        lit("%1 Connection to peer %2 canceled from state %3").
        arg(Q_FUNC_INFO).arg(m_remotePeer.id.toString()).arg(toString(state())),
        cl_logDEBUG2);
    setState(State::Error);
}

void P2pConnection::onHttpClientDone()
{
    nx_http::AsyncClient::State state = m_httpClient->state();
    if (state == nx_http::AsyncClient::sFailed)
    {
        cancelConnecting();
        return;
    }

    const int statusCode = m_httpClient->response()->statusLine.statusCode;

    NX_LOG(QnLog::P2P_TRAN_LOG, lit("P2pConnection::at_responseReceived. statusCode = %1").
        arg(statusCode), cl_logDEBUG2);

    if (statusCode == nx_http::StatusCode::unauthorized)
    {
        m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
        if (m_credentialsSource < CredentialsSource::none)
        {
            using namespace std::placeholders;
            fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey);
            m_httpClient->doPost(
                m_remotePeerUrl,
                std::bind(&P2pConnection::onHttpClientDone, this));
        }
        else
        {
            cancelConnecting();
        }
        return;
    }
    else if (!nx_http::StatusCode::isSuccessCode(statusCode)) //checking that statusCode is 2xx
    {
        cancelConnecting();
        return;
    }

    const auto& headers = m_httpClient->response()->headers;
    if (headers.find(Qn::EC2_CONNECT_STAGE_1) != headers.end())
    {
        // addition stage for server to server connect. It prevent establishing two (incoming and outgoing) connections at once
        if (m_connectionLockGuard->tryAcquireConnecting())
            m_httpClient->doGet(
                m_remotePeerUrl,
                std::bind(&P2pConnection::onHttpClientDone, this));
        else
            cancelConnecting();
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
        cancelConnecting();
        return;
    }

    m_remotePeer = remotePeer;

    NX_ASSERT(!m_remotePeer.instanceId.isNull());
    if (m_remotePeer.id == ec2::kCloudPeerId)
        m_remotePeer.peerType = Qn::PT_CloudServer;

    if (!m_connectionLockGuard->tryAcquireConnected())
    {
        cancelConnecting();
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

    NX_LOG(QnLog::P2P_TRAN_LOG,
        lit("QnTransactionTransportBase::at_httpClientDone. state = %1").
        arg((int)m_httpClient->state()), cl_logDEBUG2);

    using namespace nx::network;
    m_webSocket.reset(new websocket::WebSocket(std::move(socket)));
    m_httpClient.reset();
    setState(State::Connected);
}

void P2pConnection::startConnection()
{
    m_httpClient->doGet(
        m_remotePeerUrl,
        std::bind(&P2pConnection::onHttpClientDone, this));
}

void P2pConnection::startReading()
{
    using namespace std::placeholders;
    m_webSocket->readSomeAsync(
        &m_readBuffer,
        std::bind(&P2pConnection::onNewMessageRead, this, _1, _2));
}

P2pConnection::State P2pConnection::state() const
{
    return m_state;
}

void P2pConnection::setState(State state)
{
    if (state != m_state)
    {
        m_state = state;
        emit stateChanged(weakPointer(), state);
    }
}

void P2pConnection::sendMessage(MessageType messageType, const nx::Buffer& data)
{
    nx::Buffer buffer;
    buffer.reserve(data.size() + 1);
    buffer.append((char) messageType);
    buffer.append(data);
    sendMessage(buffer);
}

void P2pConnection::sendMessage(const nx::Buffer& data)
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
                    std::bind(&P2pConnection::onMessageSent, this, _1, _2));
            }
        }
    );
}

void P2pConnection::onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent)
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
            std::bind(&P2pConnection::onMessageSent, this, _1, _2));
    }
    else
    {
        emit allDataSent(weakPointer());
    }
}

void P2pConnection::onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead)
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
        std::bind(&P2pConnection::onNewMessageRead, this, _1, _2));
}

bool P2pConnection::handleMessage(const nx::Buffer& message)
{
    NX_ASSERT(!message.isEmpty());
    MessageType messageType = (MessageType) message[0];
    NX_ASSERT(!m_remotePeer.persistentId.isNull());
    emit gotMessage(weakPointer(), messageType, message.mid(1));
    return true;
}

ConnectionContext& P2pConnection::context()
{
    return m_context;
}

} // namespace p2p
} // namespace nx
