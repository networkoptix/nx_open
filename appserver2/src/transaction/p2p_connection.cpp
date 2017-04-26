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

namespace {

    static const QnUuid kCloudPeerId(lit("674BAFD7-4EEC-4BBA-84AA-A1BAEA7FC6DB"));

} // namespace

namespace ec2 {

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

const char* toString(MessageType value)
{
    switch (value)
    {
    case MessageType::resolvePeerNumberRequest:
        return "resolvePeerNumberRequest";
    case MessageType::resolvePeerNumberResponse:
        return "resolvePeerNumberResponse";
    case MessageType::alivePeers:
        return "alivePeers";
    case MessageType::subscribeForDataUpdates:
        return "subscribeForDataUpdates";
    case MessageType::pushTransactionData:
        return "pushTransactionData";
    default:
        return "Unknown";
    }
}

P2pConnection::P2pConnection(
    QnCommonModule* commonModule,
    const QnUuid& remoteId,
    const ApiPeerData& localPeer,
    const QUrl& _remotePeerUrl)
:
    QnCommonModuleAware(commonModule),
    m_httpClient(nx_http::AsyncHttpClient::create()),
    m_localPeer(localPeer),
    m_direction(Direction::outgoing)
{
    m_readBuffer.reserve(1024 * 1024);

    QUrl remotePeerUrl = _remotePeerUrl;
    m_remotePeer.id = remoteId;


    connect(
        m_httpClient.get(),
        &nx_http::AsyncHttpClient::responseReceived,
        this,
        &P2pConnection::onResponseReceived,
        Qt::DirectConnection);

    connect(
        m_httpClient.get(),
        &nx_http::AsyncHttpClient::done,
        this,
        &P2pConnection::onHttpClientDone,
        Qt::DirectConnection);

    if (remotePeerUrl.userName().isEmpty())
    {
        fillAuthInfo(m_httpClient, m_credentialsSource == CredentialsSource::serverKey);
    }
    else
    {
        m_credentialsSource = CredentialsSource::remoteUrl;
        m_httpClient->setUserName(remotePeerUrl.userName());
        m_httpClient->setUserPassword(remotePeerUrl.password());
    }

    if (m_localPeer.isServer())
        m_httpClient->addAdditionalHeader(
            Qn::EC2_SYSTEM_ID_HEADER_NAME,
            commonModule->globalSettings()->localSystemId().toByteArray());

    // add headers
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME,
        nx_http::header::KeepAlive(
            std::chrono::duration_cast<std::chrono::seconds>(
                commonModule->globalSettings()->connectionKeepAliveTimeout())).toString());

    m_httpClient->addAdditionalHeader(
        Qn::EC2_GUID_HEADER_NAME,
        m_localPeer.id.toByteArray());
    m_httpClient->addAdditionalHeader(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        m_localPeer.instanceId.toByteArray());
    m_httpClient->addAdditionalHeader(
        Qn::EC2_DB_GUID_HEADER_NAME,
        m_localPeer.persistentId.toByteArray());
    m_httpClient->addAdditionalHeader(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        QByteArray::number(m_localPeerProtocolVersion));

    nx_http::Request request;
    nx::network::websocket::addClientHeaders(&request, kP2pProtoName);
    m_httpClient->addRequestHeaders(request.headers);

    QUrlQuery q(remotePeerUrl.query());

#ifdef USE_JSON
    q.addQueryItem("format", QnLexical::serialized(Qn::JsonFormat));
#else
    if (m_localPeer.isMobileClient())
        q.addQueryItem("format", QnLexical::serialized(Qn::JsonFormat));
#endif
    q.addQueryItem("peerType", QnLexical::serialized(m_localPeer.peerType));

    remotePeerUrl.setQuery(q);

    m_httpClient->doGet(remotePeerUrl);
}

P2pConnection::P2pConnection(
    QnCommonModule* commonModule,
    const ApiPeerData& remotePeer,
    const ApiPeerData& localPeer,
    const WebSocketPtr& webSocket)
:
    QnCommonModuleAware(commonModule),
    m_remotePeer(remotePeer),
    m_localPeer(localPeer),
    m_webSocket(webSocket),
    m_direction(Direction::incoming)
{
    m_readBuffer.reserve(1024 * 1024);

    using namespace std::placeholders;
    setState(State::Connected);
    m_webSocket->readSomeAsync(
        &m_readBuffer,
        std::bind(&P2pConnection::onNewMessageRead, this, _1, _2));
}

P2pConnection::~P2pConnection()
{
    if (m_webSocket)
        m_webSocket->pleaseStopSync();
    if (m_httpClient)
        m_httpClient->pleaseStopSync();
}

void P2pConnection::fillAuthInfo(const nx_http::AsyncHttpClientPtr& httpClient, bool authByKey)
{
    if (!commonModule()->videowallGuid().isNull())
    {
        httpClient->addAdditionalHeader(
            "X-NetworkOptix-VideoWall",
            commonModule()->videowallGuid().toString().toUtf8());
        return;
    }

    const auto& resPool = commonModule()->resourcePool();
    QnMediaServerResourcePtr ownServer =
        resPool->getResourceById<QnMediaServerResource>(localPeer().id);
    if (ownServer && authByKey)
    {
        httpClient->setUserName(ownServer->getId().toString().toLower());
        httpClient->setUserPassword(ownServer->getAuthKey());
    }
    else
    {
        QUrl url;
        if (const auto& connection = commonModule()->ec2Connection())
            url = connection->connectionInfo().ecUrl;
        httpClient->setUserName(url.userName().toLower());
        if (ApiPeerData::isServer(localPeer().peerType))
        {
            // try auth by admin user if allowed
            QnUserResourcePtr adminUser = resPool->getAdministrator();
            if (adminUser)
            {
                httpClient->setUserPassword(adminUser->getDigest());
                httpClient->setAuthType(nx_http::AsyncHttpClient::authDigestWithPasswordHash);
            }
        }
        else
        {
            httpClient->setUserPassword(url.password());
        }
    }
}

void P2pConnection::cancelConnecting()
{
    NX_LOG(
        lit("%1 Connection to peer %2 canceled from state %3").
        arg(Q_FUNC_INFO).arg(m_remotePeer.id.toString()).arg(toString(state())),
        cl_logDEBUG1);
    setState(State::Error);
}

void P2pConnection::onResponseReceived(const nx_http::AsyncHttpClientPtr& client)
{
    const int statusCode = client->response()->statusLine.statusCode;

    NX_LOG( lit("P2pConnection::at_responseReceived. statusCode = %1").
        arg(statusCode), cl_logDEBUG2);

    if (statusCode == nx_http::StatusCode::unauthorized)
    {
        m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
        if (m_credentialsSource < CredentialsSource::none)
        {
            fillAuthInfo(m_httpClient, m_credentialsSource == CredentialsSource::serverKey);
            m_httpClient->doGet(m_httpClient->url());
        }
        else
        {
            cancelConnecting();
        }
        return;
    }

    if (!client->response())
    {
        cancelConnecting();
        return;
    }

    nx_http::HttpHeaders::const_iterator itrGuid = client->response()->headers.find(Qn::EC2_GUID_HEADER_NAME);
    nx_http::HttpHeaders::const_iterator itrRuntimeGuid = client->response()->headers.find(Qn::EC2_RUNTIME_GUID_HEADER_NAME);
    nx_http::HttpHeaders::const_iterator itrSystemIdentityTime = client->response()->headers.find(Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME);
    if (itrSystemIdentityTime != client->response()->headers.end())
        setRemoteIdentityTime(itrSystemIdentityTime->second.toLongLong());

    if (itrGuid == client->response()->headers.end())
    {
        cancelConnecting();
        return;
    }

    nx_http::HttpHeaders::const_iterator ec2CloudHostItr =
        client->response()->headers.find(Qn::EC2_CLOUD_HOST_HEADER_NAME);

    if (!localPeer().isMobileClient())
    {
        //checking remote server protocol version
        nx_http::HttpHeaders::const_iterator ec2ProtoVersionIter =
            client->response()->headers.find(Qn::EC2_PROTO_VERSION_HEADER_NAME);

        m_remotePeerEcProtoVersion =
            ec2ProtoVersionIter == client->response()->headers.end()
            ? nx_ec::INITIAL_EC2_PROTO_VERSION
            : ec2ProtoVersionIter->second.toInt();

        if (m_localPeerProtocolVersion != m_remotePeerEcProtoVersion)
        {
            NX_LOG(lm("Cannot connect to server %1 because of different EC2 proto version. "
                "Local peer version: %2, remote peer version: %3")
                .arg(client->url()).arg(m_localPeerProtocolVersion)
                .arg(m_remotePeerEcProtoVersion),
                cl_logWARNING);
            cancelConnecting();
            return;
        }

        const QString remotePeerCloudHost = ec2CloudHostItr == client->response()->headers.end()
            ? nx::network::AppInfo::defaultCloudHost()
            : QString::fromUtf8(ec2CloudHostItr->second);

        if (nx::network::AppInfo::defaultCloudHost() != remotePeerCloudHost)
        {
            NX_LOG(lm("Cannot connect to server %1 because they have different built in cloud host setting. "
                "Local peer host: %2, remote peer host: %3").
                arg(client->url()).arg(nx::network::AppInfo::defaultCloudHost()).arg(remotePeerCloudHost),
                cl_logWARNING);
            cancelConnecting();
            return;
        }
    }

    m_remotePeer.id = QnUuid(itrGuid->second);
    if (itrRuntimeGuid != client->response()->headers.end())
        m_remotePeer.instanceId = QnUuid(itrRuntimeGuid->second);

    NX_ASSERT(!m_remotePeer.instanceId.isNull());
    if (m_remotePeer.id == kCloudPeerId)
        m_remotePeer.peerType = Qn::PT_CloudServer;
    else if (ec2CloudHostItr == client->response()->headers.end())
        m_remotePeer.peerType = Qn::PT_OldServer; // outgoing connections for server or cloud peers only
    else
        m_remotePeer.peerType = Qn::PT_Server; // outgoing connections for server or cloud peers only
#ifdef USE_JSON
    m_remotePeer.dataFormat = Qn::JsonFormat;
#else
    if (m_localPeer.isMobileClient())
        m_remotePeer.dataFormat = Qn::JsonFormat;
    else
        m_remotePeer.dataFormat = Qn::UbjsonFormat;
#endif

    if (!nx_http::StatusCode::isSuccessCode(statusCode)) //checking that statusCode is 2xx
    {
        cancelConnecting();
        return;
    }
}

void P2pConnection::onHttpClientDone(const nx_http::AsyncHttpClientPtr& client)
{
    QnMutexLocker lock(&m_mutex);

    using namespace nx::network;

    NX_LOG( lit("QnTransactionTransportBase::at_httpClientDone. state = %1").
        arg((int)client->state()), cl_logDEBUG2);

    nx_http::AsyncHttpClient::State state = client->state();
    if (state == nx_http::AsyncHttpClient::sFailed) {
        cancelConnecting();
        return;
    }

    auto error = websocket::validateResponse(client->request(), *client->response());
    if (error != websocket::Error::noError)
    {
        NX_LOG(lm("Can't establish WEB socket connection. Validation failed. Error: %1").arg((int) error), cl_logERROR);
        setState(State::Error);
        m_httpClient.reset();
        return;
    }

    auto msgBuffer = client->fetchMessageBodyBuffer();

    auto socket = m_httpClient->takeSocket();
    auto keepAliveTimeout = commonModule()->globalSettings()->connectionKeepAliveTimeout() * 1000;
    socket->setRecvTimeout(std::chrono::milliseconds(keepAliveTimeout * 2).count());
    socket->setSendTimeout(std::chrono::milliseconds(keepAliveTimeout * 2).count());
    socket->setNonBlockingMode(true);
    socket->setNoDelay(true);

    m_webSocket.reset(new WebSocket( std::move(socket), msgBuffer));
    //m_webSocket = std::move(socket);
    NX_ASSERT(msgBuffer.isEmpty());

    m_httpClient.reset();
    setState(State::Connected);

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
    m_state = state;
}

void P2pConnection::setRemoteIdentityTime(qint64 time)
{
    m_remoteIdentityTime = time;
}

qint64 P2pConnection::remoteIdentityTime() const
{
    return m_remoteIdentityTime;
}

ApiPeerData P2pConnection::localPeer() const
{
    return m_localPeer;
}

ApiPeerData P2pConnection::remotePeer() const
{
    return m_remotePeer;
}

P2pConnection::Direction P2pConnection::direction() const
{
    return m_direction;
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
    if (QnLog::instance()->logLevel() >= cl_logDEBUG1)
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
        auto remotePeerName = qnStaticCommon->moduleDisplayName(remotePeer().id);

        MessageType messageType = (MessageType)data[0];
        NX_LOG(lit("Send message:\t %1 ---> %2. Type: %3. Size=%4")
            .arg(localPeerName)
            .arg(remotePeerName)
            .arg(toString(messageType))
            .arg(data.size()),
            cl_logDEBUG1);
    }

    using namespace std::placeholders;

    QnMutexLocker lock(&m_mutex);
    m_dataToSend.push_back(data);
    if (m_dataToSend.size() == 1)
        m_webSocket->sendAsync(
            m_dataToSend.front(),
            std::bind(&P2pConnection::onMessageSent, this, _1, _2));
}

void P2pConnection::onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent)
{
    QnMutexLocker lock(&m_mutex);

    if (errorCode != SystemError::noError)
        setState(State::Error);
    if (bytesSent == 0)
    {
        setState(State::Error);
        return;
    }
    using namespace std::placeholders;
    m_dataToSend.pop_front();
    if (!m_dataToSend.empty())
        m_webSocket->sendAsync(
            m_dataToSend.front(),
            std::bind(&P2pConnection::onMessageSent, this, _1, _2));
}

void P2pConnection::onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead)
{
    QnMutexLocker lock(&m_mutex);
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
    NX_ASSERT(message.size() > 1);
    MessageType messageType = (MessageType) message[0];
    emit gotMessage(toSharedPointer(), messageType, message.mid(1));
    return true;
}

P2pConnection::MiscData& P2pConnection::miscData()
{
    return m_miscData;
}

ApiPersistentIdData P2pConnection::decode(PeerNumberType shortPeerNumber) const
{
    return m_shortPeerInfo.decode(shortPeerNumber);
}

PeerNumberType P2pConnection::encode(const ApiPersistentIdData& fullId, PeerNumberType shortPeerNumber)
{
    return m_shortPeerInfo.encode(fullId, shortPeerNumber);
}

bool P2pConnection::isSubscribedTo(const ApiPersistentIdData& peer)
{
    const auto& data = m_miscData.remoteSubscription;
    auto itr = std::find(data.begin(), data.end(), peer);
    return itr != data.end();
}

} // namespace ec2
