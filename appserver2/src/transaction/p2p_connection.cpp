#include "p2p_connection.h"
#include <nx/utils/log/log_message.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <http/custom_headers.h>

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

P2pConnection::P2pConnection(
    QnCommonModule* commonModule,
    const QnUuid& id,
    const QUrl& url)
:
    QnCommonModuleAware(commonModule),
    m_httpClient(nx_http::AsyncHttpClient::create()),
    m_direction(Direction::outgoing)
{
    m_remotePeer.id = id;

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

    nx_http::Request request;
    nx::network::websocket::addClientHeaders(&request, kP2pProtoName);
    m_httpClient->addRequestHeaders(request.headers);
    m_httpClient->doGet(url);
}

P2pConnection::P2pConnection(
    QnCommonModule* commonModule,
    const ApiPeerData& remotePeer,
    const WebSocketPtr& webSocket)
:
    QnCommonModuleAware(commonModule),
    m_remotePeer(remotePeer),
    m_webSocket(webSocket),
    m_direction(Direction::incoming)
{
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
    NX_LOG(QnLog::EC2_TRAN_LOG,
        lit("%1 Connection to peer %2 canceled from state %3").
        arg(Q_FUNC_INFO).arg(m_remotePeer.id.toString()).arg(toString(state())),
        cl_logDEBUG1);
    setState(State::Error);
}

void P2pConnection::onResponseReceived(const nx_http::AsyncHttpClientPtr& client)
{
    const int statusCode = client->response()->statusLine.statusCode;

    NX_LOG(QnLog::EC2_TRAN_LOG, lit("P2pConnection::at_responseReceived. statusCode = %1").
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
    using namespace nx::network;

    NX_LOG(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransportBase::at_httpClientDone. state = %1").
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
    m_webSocket.reset(new nx::network::websocket::Websocket(
        m_httpClient->takeSocket(),
        msgBuffer));
    m_httpClient.reset();
    setState(State::Connected);
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
    return m_remotePeer;
}

ApiPeerData P2pConnection::remotePeer() const
{
    return m_localPeer;
}

P2pConnection::Direction P2pConnection::direction() const
{
    return m_direction;
}

void P2pConnection::unsubscribeFrom(const ApiPeerIdData& idList)
{
    // todo: implement me
}

void P2pConnection::subscribeTo(const std::vector<ApiPeerIdData>& idList)
{
    // todo: implement me
}

void P2pConnection::sendMessage(MessageType messageType, const QByteArray& data)
{
    // todo: implement me
}

} // namespace ec2
