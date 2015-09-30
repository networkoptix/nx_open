#include "ec2_transaction_tcp_listener.h"

#include <QtCore/QUrlQuery>

#include <nx_ec/ec_proto_version.h>

#include "utils/network/tcp_connection_priv.h"
#include "transaction/transaction_message_bus.h"
#include "nx_ec/data/api_full_info_data.h"
#include "database/db_manager.h"
#include "common/common_module.h"
#include "transaction/transaction_transport.h"
#include "http/custom_headers.h"
#include "audit/audit_manager.h"


namespace ec2
{

// -------------------------- QnTransactionTcpProcessor ---------------------


class QnTransactionTcpProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:

    QnTransactionTcpProcessorPrivate(): 
        QnTCPConnectionProcessorPrivate()
    {
    }
};

QnTransactionTcpProcessor::QnTransactionTcpProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnTransactionTcpProcessorPrivate, socket)
{
    Q_UNUSED(_owner)

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

    QUrlQuery query = QUrlQuery(d->request.requestLine.url.query());
    bool isClient = query.hasQueryItem("isClient");
    bool isMobileClient = query.hasQueryItem("isMobile");
    QnUuid remoteGuid  = QnUuid(query.queryItemValue("guid"));
    QnUuid remoteRuntimeGuid  = QnUuid(query.queryItemValue("runtime-guid"));
    qint64 remoteSystemIdentityTime  = query.queryItemValue("system-identity-time").toLongLong();
    if (remoteGuid.isNull())
        remoteGuid = QnUuid::createUuid();
    QnUuid videowallGuid = QnUuid(query.queryItemValue("videowallGuid"));
    bool isVideowall = (!videowallGuid.isNull());

    Qn::PeerType peerType = isMobileClient  ? Qn::PT_MobileClient
        : isVideowall   ? Qn::PT_VideowallClient
        : isClient      ? Qn::PT_DesktopClient
        : Qn::PT_Server;

    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
    if (query.hasQueryItem("format"))
         QnLexical::deserialize(query.queryItemValue("format"), &dataFormat);

    ApiPeerData remotePeer(remoteGuid, remoteRuntimeGuid, peerType, dataFormat);


    if( d->request.requestLine.method == nx_http::Method::POST ||
        d->request.requestLine.method == nx_http::Method::PUT )
    {
        auto connectionGuidIter = d->request.headers.find( Qn::EC2_CONNECTION_GUID_HEADER_NAME );
        if( connectionGuidIter == d->request.headers.end() )
        {
            sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
            return;
        }
        const QnUuid connectionGuid( connectionGuidIter->second );

        auto connectionDirectionIter = d->request.headers.find( Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME );
        if( connectionDirectionIter == d->request.headers.end() )
        {
            sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
            return;
        }
        const ConnectionType::Type connectionDirection = ConnectionType::fromString( connectionDirectionIter->second );
        if( connectionDirection != ConnectionType::outgoing )
        {
            sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
            return;
        }

        //we must pass socket to QnTransactionMessageBus atomically, but QSharedPointer has move operator only,
        //  but not move initializer. So, have to declare localSocket
        auto localSocket = std::move(d->socket);
        d->socket.clear();
        QnTransactionMessageBus::instance()->gotIncomingTransactionsConnectionFromRemotePeer(
            connectionGuid,
            std::move(localSocket),
            remotePeer,
            remoteSystemIdentityTime,
            d->request,
            d->clientRequest );
        sendResponse( nx_http::StatusCode::ok, nx_http::StringType() );
        return;
    }


    d->response.headers.insert(nx_http::HttpHeader(Qn::EC2_GUID_HEADER_NAME, qnCommon->moduleGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader(Qn::EC2_RUNTIME_GUID_HEADER_NAME, qnCommon->runningInstanceGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader(Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME, QByteArray::number(qnCommon->systemIdentityTime())));
    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        nx_http::StringType::number(nx_ec::EC2_PROTO_VERSION)));
    d->response.headers.insert(nx_http::HttpHeader(
        Qn::EC2_SYSTEM_NAME_HEADER_NAME,
        QnCommonModule::instance()->localSystemName().toUtf8()));

    auto systemNameHeaderIter = d->request.headers.find(Qn::EC2_SYSTEM_NAME_HEADER_NAME);
    if( (systemNameHeaderIter != d->request.headers.end()) &&
        (QString::fromUtf8(nx_http::getHeaderValue(d->request.headers, Qn::EC2_SYSTEM_NAME_HEADER_NAME)) != 
            QnCommonModule::instance()->localSystemName()) )
    {
        sendResponse(nx_http::StatusCode::forbidden, nx_http::StringType());
        return;
    }

    if (remotePeer.peerType == Qn::PT_Server)
    {
        // use two stage connect for server peers only, go to second stage for client immediately

        // 1-st stage
        bool lockOK = QnTransactionTransport::tryAcquireConnecting(remoteGuid, false);
        d->response.headers.emplace( "Content-Length", "0" );   //only declaring content-type
        sendResponse(
            lockOK ? nx_http::StatusCode::noContent : nx_http::StatusCode::forbidden,
            QnTransactionTransport::TUNNEL_CONTENT_TYPE );
        if (!lockOK)
            return;

        // 2-nd stage
        if (!readRequest()) {
            QnTransactionTransport::connectingCanceled(remoteGuid, false);
            return;
        }
        parseRequest();

        d->response.headers.insert(nx_http::HttpHeader(Qn::EC2_GUID_HEADER_NAME, qnCommon->moduleGUID().toByteArray()));
        d->response.headers.insert(nx_http::HttpHeader(Qn::EC2_RUNTIME_GUID_HEADER_NAME, qnCommon->runningInstanceGUID().toByteArray()));
        d->response.headers.insert(nx_http::HttpHeader(Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME, QByteArray::number(qnCommon->systemIdentityTime())));
        d->response.headers.insert(nx_http::HttpHeader(
            Qn::EC2_PROTO_VERSION_HEADER_NAME,
            nx_http::StringType::number(nx_ec::EC2_PROTO_VERSION)));
        d->response.headers.insert(nx_http::HttpHeader(
            Qn::EC2_SYSTEM_NAME_HEADER_NAME,
            QnCommonModule::instance()->localSystemName().toUtf8()));

        auto systemNameHeaderIter = d->request.headers.find(Qn::EC2_SYSTEM_NAME_HEADER_NAME);
        if( (systemNameHeaderIter != d->request.headers.end()) &&
            (QString::fromUtf8(nx_http::getHeaderValue(d->request.headers, Qn::EC2_SYSTEM_NAME_HEADER_NAME)) != 
                QnCommonModule::instance()->localSystemName()) )
        {
            sendResponse(nx_http::StatusCode::forbidden, nx_http::StringType());
            return;
        }
    }

    QnUuid connectionGuid;
    auto connectionGuidIter = d->request.headers.find( Qn::EC2_CONNECTION_GUID_HEADER_NAME );
    if( connectionGuidIter == d->request.headers.end() )
        connectionGuid = QnUuid::createUuid();  //generating random connection guid
    else
        connectionGuid = connectionGuidIter->second;

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
        nx_http::header::AcceptEncodingHeader acceptEncodingHeader( acceptEncodingHeaderIter->second );
        if( acceptEncodingHeader.encodingIsAllowed( "identity" ) )
            contentEncoding = "identity";
        else if( acceptEncodingHeader.encodingIsAllowed( "gzip" ) )
            contentEncoding = "gzip";
        //else
            //TODO #ak not supported encoding requested
    }

    query = QUrlQuery(d->request.requestLine.url.query());
    bool fail = query.hasQueryItem("canceled") || !QnTransactionTransport::tryAcquireConnected(remoteGuid, false);

    if (!qnCommon->allowedPeers().isEmpty() && !qnCommon->allowedPeers().contains(remotePeer.id) && !isClient)
        fail = true; // accept only allowed peers

    d->chunkedMode = false;
    d->response.headers.emplace( "Connection", "close" );
    if( fail )
    {
        QnTransactionTransport::connectingCanceled(remoteGuid, false);
        sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
    }
    else
    {
        std::function<void ()> ttFinishCallback;
        if (remotePeer.isClient()) {
            auto session = authSession();
            //session.userAgent = toString(remotePeer.peerType);
            qnAuditManager->at_connectionOpened(session);
            ttFinishCallback = std::bind(&QnAuditManager::at_connectionClosed, qnAuditManager, session);
        }

        auto base64EncodingRequiredHeaderIter = d->request.headers.find( Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME );
        if( base64EncodingRequiredHeaderIter != d->request.headers.end() )
            d->response.headers.insert( *base64EncodingRequiredHeaderIter );

        QnTransactionMessageBus::instance()->gotConnectionFromRemotePeer(
            connectionGuid,
            std::move(d->socket),
            requestedConnectionType,
            remotePeer,
            remoteSystemIdentityTime,
            d->request,
            contentEncoding,
            ttFinishCallback
            );
        sendResponse( nx_http::StatusCode::ok, QnTransactionTransport::TUNNEL_CONTENT_TYPE, contentEncoding );

        QnTransactionMessageBus::instance()->moveConnectionToReadyForStreaming( connectionGuid );

        d->socket.clear();
    }
}


}
