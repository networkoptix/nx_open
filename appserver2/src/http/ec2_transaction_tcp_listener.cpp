#include "ec2_transaction_tcp_listener.h"

#include <QtCore/QUrlQuery>

#include <nx_ec/ec_proto_version.h>

#include "utils/network/tcp_connection_priv.h"
#include "transaction/transaction_message_bus.h"
#include "nx_ec/data/api_full_info_data.h"
#include "database/db_manager.h"
#include "common/common_module.h"
#include "transaction/transaction_transport.h"

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

    d->response.headers.insert(nx_http::HttpHeader("guid", qnCommon->moduleGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader("runtime-guid", qnCommon->runningInstanceGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader(
        nx_ec::EC2_PROTO_VERSION_HEADER_NAME,
        nx_http::StringType::number(nx_ec::EC2_PROTO_VERSION)));

    if (remotePeer.peerType == Qn::PT_Server)
    {
        // use two stage connect for server peers only, go to second stage for client immediately

        // 1-st stage
        bool lockOK = QnTransactionTransport::tryAcquireConnecting(remoteGuid, false);
        sendResponse(lockOK ? CODE_OK : CODE_INVALID_PARAMETER , "application/octet-stream");
        if (!lockOK)
            return;

        // 2-nd stage
        if (!readRequest()) {
            QnTransactionTransport::connectingCanceled(remoteGuid, false);
            return;
        }
        parseRequest();

        d->response.headers.insert(nx_http::HttpHeader("guid", qnCommon->moduleGUID().toByteArray()));
        d->response.headers.insert(nx_http::HttpHeader("runtime-guid", qnCommon->runningInstanceGUID().toByteArray()));
        d->response.headers.insert(nx_http::HttpHeader(
            nx_ec::EC2_PROTO_VERSION_HEADER_NAME,
            nx_http::StringType::number(nx_ec::EC2_PROTO_VERSION)));
    }

    query = QUrlQuery(d->request.requestLine.url.query());
    bool fail = query.hasQueryItem("canceled") || !QnTransactionTransport::tryAcquireConnected(remoteGuid, false);

    if (!qnCommon->allowedPeers().isEmpty() && !qnCommon->allowedPeers().contains(remotePeer.id))
        fail = true; // accept only allowed peers

    d->chunkedMode = true;
    sendResponse(fail ? CODE_INVALID_PARAMETER : CODE_OK, "application/octet-stream");
    if (fail) {
        QnTransactionTransport::connectingCanceled(remoteGuid, false);
    }
    else {
        QnTransactionMessageBus::instance()->gotConnectionFromRemotePeer(d->socket, remotePeer);
        d->socket.clear();
    }
}


}
