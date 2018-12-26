#include "ec2_transaction_tcp_listener.h"

#include <QtCore/QUrlQuery>

#include <api/global_settings.h>
#include <nx_ec/ec_proto_version.h>

#include "network/tcp_connection_priv.h"
#include "transaction/server_transaction_message_bus.h"
#include <database/db_manager.h>
#include "common/common_module.h"
#include "transaction/transaction_transport.h"
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/socket_global.h>
#include "audit/audit_manager.h"
#include <core/resource/media_server_resource.h>
#include <nx/fusion/serialization/lexical.h>

using namespace nx;

namespace ec2
{

// -------------------------- QnTransactionTcpProcessor ---------------------


class QnTransactionTcpProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:

    QnTransactionTcpProcessorPrivate():
        QnTCPConnectionProcessorPrivate(),
        messageBus(nullptr)
    {
    }

    ServerTransactionMessageBus* messageBus;
};

QnTransactionTcpProcessor::QnTransactionTcpProcessor(
    ServerTransactionMessageBus* messageBus,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(new QnTransactionTcpProcessorPrivate, std::move(socket), owner)
{
    Q_D(QnTransactionTcpProcessor);
    d->messageBus = messageBus;
    setObjectName( "QnTransactionTcpProcessor" );
}

QnTransactionTcpProcessor::~QnTransactionTcpProcessor()
{
    stop();
}

void QnTransactionTcpProcessor::run()
{
    Q_D(QnTransactionTcpProcessor);
    initSystemThreadId();

    if (d->clientRequest.isEmpty()) {
        if (!readRequest())
            return;
    }
    parseRequest();


    if (commonModule()->isStandAloneMode())
    {
        NX_DEBUG(this, "Incoming messageBus connections are temporary disabled. Ignore new incoming connection.");
        sendResponse(nx::network::http::StatusCode::forbidden, nx::network::http::StringType());
        return;
    }

    QUrlQuery query = QUrlQuery(d->request.requestLine.url.query());

    QnUuid remoteGuid = QnUuid(query.queryItemValue("guid"));
    if (remoteGuid.isNull())
        remoteGuid = QnUuid::createUuid();
    QnUuid remoteRuntimeGuid = QnUuid(query.queryItemValue("runtime-guid"));
    qint64 remoteSystemIdentityTime = query.queryItemValue("system-identity-time").toLongLong();

    bool deserialized = false;
    vms::api::PeerType peerType = QnLexical::deserialized<vms::api::PeerType>(
        query.queryItemValue("peerType"),
        vms::api::PeerType::notDefined,
        &deserialized);
    if (!deserialized || peerType == vms::api::PeerType::notDefined)
    {
        QnUuid videowallGuid = QnUuid(query.queryItemValue("videowallGuid"));
        const bool isVideowall = !videowallGuid.isNull();
        const bool isClient = query.hasQueryItem("isClient");
        const bool isMobileClient = query.hasQueryItem("isMobile");

        peerType = isMobileClient ? vms::api::PeerType::oldMobileClient
            : isVideowall ? vms::api::PeerType::videowallClient
            : isClient ? vms::api::PeerType::desktopClient
            : vms::api::PeerType::server;
    }

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem("format"))
         QnLexical::deserialize(query.queryItemValue("format"), &dataFormat);

    vms::api::PeerData remotePeer(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);

    if (peerType == vms::api::PeerType::server && commonModule()->isReadOnly())
    {
        sendResponse(nx::network::http::StatusCode::forbidden, nx::network::http::StringType());
        return;
    }

    const auto& commonModule = d->messageBus->commonModule();
    d->response.headers.emplace(
        Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME,
        nx::network::http::header::KeepAlive(
            commonModule->globalSettings()->connectionKeepAliveTimeout()).toString());

    if( d->request.requestLine.method == nx::network::http::Method::post ||
        d->request.requestLine.method == nx::network::http::Method::put )
    {
        auto connectionGuidIter = d->request.headers.find( Qn::EC2_CONNECTION_GUID_HEADER_NAME );
        if( connectionGuidIter == d->request.headers.end() )
        {
            sendResponse( nx::network::http::StatusCode::forbidden, nx::network::http::StringType() );
            return;
        }
        const std::string connectionGuid = connectionGuidIter->second.toStdString();

        auto connectionDirectionIter = d->request.headers.find( Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME );
        if( connectionDirectionIter == d->request.headers.end() )
        {
            sendResponse( nx::network::http::StatusCode::forbidden, nx::network::http::StringType() );
            return;
        }
        const ConnectionType::Type connectionDirection = ConnectionType::fromString( connectionDirectionIter->second );
        if( connectionDirection != ConnectionType::outgoing )
        {
            sendResponse( nx::network::http::StatusCode::forbidden, nx::network::http::StringType() );
            return;
        }

        d->messageBus->gotIncomingTransactionsConnectionFromRemotePeer(
            connectionGuid,
            std::move(d->socket),
            remotePeer,
            remoteSystemIdentityTime,
            d->request,
            d->clientRequest );
        sendResponse( nx::network::http::StatusCode::ok, nx::network::http::StringType() );
        return;
    }

    d->response.headers.insert(nx::network::http::HttpHeader(
        Qn::EC2_GUID_HEADER_NAME,
        commonModule->moduleGUID().toByteArray()));
    d->response.headers.insert(nx::network::http::HttpHeader(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        commonModule->runningInstanceGUID().toByteArray()));
    d->response.headers.insert(nx::network::http::HttpHeader(
        Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME,
        QByteArray::number(commonModule->systemIdentityTime())));
    d->response.headers.insert(nx::network::http::HttpHeader(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        nx::network::http::StringType::number(nx_ec::EC2_PROTO_VERSION)));
    d->response.headers.insert(nx::network::http::HttpHeader(
        Qn::EC2_CLOUD_HOST_HEADER_NAME,
        nx::network::SocketGlobals::cloud().cloudHost().toUtf8()));
    d->response.headers.insert(nx::network::http::HttpHeader(
        Qn::EC2_SYSTEM_ID_HEADER_NAME,
        commonModule->globalSettings()->localSystemId().toByteArray()));

    auto systemNameHeaderIter = d->request.headers.find(Qn::EC2_SYSTEM_ID_HEADER_NAME);
    if( (systemNameHeaderIter != d->request.headers.end()) &&
        (nx::network::http::getHeaderValue(d->request.headers, Qn::EC2_SYSTEM_ID_HEADER_NAME) !=
            commonModule->globalSettings()->localSystemId().toByteArray()) )
    {
        sendResponse(nx::network::http::StatusCode::forbidden, nx::network::http::StringType());
        return;
    }

    ConnectionLockGuard connectionLockGuard(
        commonModule->moduleGUID(),
        d->messageBus->connectionGuardSharedState(),
        remoteGuid,
        ConnectionLockGuard::Direction::Incoming);

    if (remotePeer.peerType == vms::api::PeerType::server)
    {
        // use two stage connect for server peers only, go to second stage for client immediately

        // 1-st stage
        bool lockOK = connectionLockGuard.tryAcquireConnecting();

        d->response.headers.emplace( "Content-Length", "0" );   //only declaring content-type
        sendResponse(
            lockOK ? nx::network::http::StatusCode::noContent : nx::network::http::StatusCode::forbidden,
            QnTransactionTransport::TUNNEL_CONTENT_TYPE );
        if (!lockOK)
            return;

        // 2-nd stage
        if (!readRequest())
            return;

        parseRequest();

        d->response.headers.insert(nx::network::http::HttpHeader(
            Qn::EC2_GUID_HEADER_NAME,
            commonModule->moduleGUID().toByteArray()));
        d->response.headers.insert(nx::network::http::HttpHeader(
            Qn::EC2_RUNTIME_GUID_HEADER_NAME,
            commonModule->runningInstanceGUID().toByteArray()));
        d->response.headers.insert(nx::network::http::HttpHeader(
            Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME,
            QByteArray::number(commonModule->systemIdentityTime())));
        d->response.headers.insert(nx::network::http::HttpHeader(
            Qn::EC2_PROTO_VERSION_HEADER_NAME,
            nx::network::http::StringType::number(nx_ec::EC2_PROTO_VERSION)));
        d->response.headers.insert(nx::network::http::HttpHeader(
            Qn::EC2_CLOUD_HOST_HEADER_NAME,
            nx::network::SocketGlobals::cloud().cloudHost().toUtf8()));

        d->response.headers.insert(nx::network::http::HttpHeader(
            Qn::EC2_SYSTEM_ID_HEADER_NAME,
            commonModule->globalSettings()->localSystemId().toByteArray()));

        auto systemNameHeaderIter = d->request.headers.find(Qn::EC2_SYSTEM_ID_HEADER_NAME);
        if( (systemNameHeaderIter != d->request.headers.end()) &&
            (nx::network::http::getHeaderValue(d->request.headers, Qn::EC2_SYSTEM_ID_HEADER_NAME) !=
                commonModule->globalSettings()->localSystemId().toByteArray()) )
        {
            sendResponse(nx::network::http::StatusCode::forbidden, nx::network::http::StringType());
            return;
        }

        d->response.headers.emplace(
            Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME,
            nx::network::http::header::KeepAlive(
                commonModule->globalSettings()->connectionKeepAliveTimeout()).toString());
    }

    std::string connectionGuid;
    auto connectionGuidIter = d->request.headers.find( Qn::EC2_CONNECTION_GUID_HEADER_NAME );
    if( connectionGuidIter == d->request.headers.end() )
        connectionGuid = QnUuid::createUuid().toSimpleString().toStdString();  //generating random connection guid
    else
        connectionGuid = connectionGuidIter->second.toStdString();

    ConnectionType::Type requestedConnectionType = ConnectionType::none;
    auto connectionDirectionIter = d->request.headers.find( Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME );
    if( connectionDirectionIter == d->request.headers.end() )
        requestedConnectionType = ConnectionType::incoming;
    else
        requestedConnectionType = ConnectionType::fromString(connectionDirectionIter->second);

    //checking content encoding requested by remote peer
    auto acceptEncodingHeaderIter = d->request.headers.find( "Accept-Encoding" );
    QByteArray contentEncoding;
    if( acceptEncodingHeaderIter != d->request.headers.end() )
    {
        nx::network::http::header::AcceptEncodingHeader acceptEncodingHeader( acceptEncodingHeaderIter->second );
        if( acceptEncodingHeader.encodingIsAllowed( "identity" ) )
            contentEncoding = "identity";
        else if( acceptEncodingHeader.encodingIsAllowed( "gzip" ) )
            contentEncoding = "gzip";
        //else
            //TODO #ak not supported encoding requested
    }

    query = QUrlQuery(d->request.requestLine.url.query());

    bool fail = query.hasQueryItem("canceled");
    if (!fail)
    {
        fail = !connectionLockGuard.tryAcquireConnected();
    }

    if (!commonModule->allowedPeers().isEmpty() && !commonModule->allowedPeers().contains(remotePeer.id) && !remotePeer.isClient())
        fail = true; // accept only allowed peers

    if (remotePeer.id == commonModule->moduleGUID())
        fail = true; //< Rejecting connect from ourselves.

    d->chunkedMode = false;
    //d->response.headers.emplace( "Connection", "close" );
    if( fail )
    {
        sendResponse( nx::network::http::StatusCode::forbidden, nx::network::http::StringType() );
    }
    else
    {
        std::function<void ()> ttFinishCallback;
        if (remotePeer.isClient()) {
            auto session = authSession();
            //session.userAgent = toString(remotePeer.peerType);
            commonModule->auditManager()->at_connectionOpened(session);
            ttFinishCallback = std::bind(&QnAuditManager::at_connectionClosed, commonModule->auditManager(), session);
        }

        auto base64EncodingRequiredHeaderIter = d->request.headers.find( Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME );
        if( base64EncodingRequiredHeaderIter != d->request.headers.end() )
            d->response.headers.insert( *base64EncodingRequiredHeaderIter );


        sendResponse( nx::network::http::StatusCode::ok, QnTransactionTransport::TUNNEL_CONTENT_TYPE, contentEncoding );

        // By default all peers have read permissions on all resources
        auto access = d->accessRights;
        if (remotePeer.peerType == vms::api::PeerType::server)
        {
            // Here we substitute admin user with SuperAccess user to pass by all access checks unhurt
            // since server-to-server order of transactions is unpredictable and access check for resource attribute
            // may come before resource itself is added to the resource pool and this may be restricted by the access
            // checking mechanics.
            if (access != Qn::kSystemAccess)
            {
                bool authAsOwner = d->accessRights.userId == QnUserResource::kAdminGuid;
                NX_ASSERT(authAsOwner, "Server must always be authorised as owner");
                if (authAsOwner)
                    access = Qn::kSystemAccess;
            }
        }
        else
        {
            access.access = Qn::UserAccessData::Access::ReadAllResources;
        }

        d->messageBus->gotConnectionFromRemotePeer(
            connectionGuid,
            std::move(connectionLockGuard),
            std::move(d->socket),
            requestedConnectionType,
            remotePeer,
            remoteSystemIdentityTime,
            d->request,
            contentEncoding,
            ttFinishCallback,
            access);

        d->messageBus->moveConnectionToReadyForStreaming(connectionGuid);
    }
}


} // namespace ec2
