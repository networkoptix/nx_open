#include "ec2_transaction_tcp_listener.h"

#include <QtCore/QUrlQuery>

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
    QUuid remoteGuid  = query.queryItemValue(lit("guid"));
    if (remoteGuid.isNull())
        remoteGuid = QUuid::createUuid();
    QnPeerInfo remotePeer(remoteGuid, isMobileClient 
        ? QnPeerInfo::AndroidClient
        : isClient 
        ? QnPeerInfo::DesktopClient
        : QnPeerInfo::Server);

    QByteArray remoteHwList = query.queryItemValue(lit("hwList")).toLocal8Bit();

    d->response.headers.insert(nx_http::HttpHeader("guid", qnCommon->moduleGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader("hwList", QnTransactionTransport::encodeHWList(qnLicensePool->allLocalHardwareIds())));

    if (!isClient)
    {
        // use two stage connect for server peers only, go to second stage for client immediatly

        // 1-st stage
        bool lockOK = QnTransactionTransport::tryAcquireConnecting(remoteGuid, false);
        sendResponse("HTTP", lockOK ? CODE_OK : CODE_INVALID_PARAMETER , "application/octet-stream");
        if (!lockOK)
            return;

        // 2-nd stage
        if (!readRequest()) {
            QnTransactionTransport::connectingCanceled(remoteGuid, false);
            return;
        }
        parseRequest();

        d->response.headers.insert(nx_http::HttpHeader("guid", qnCommon->moduleGUID().toByteArray()));
        d->response.headers.insert(nx_http::HttpHeader("hwList", QnTransactionTransport::encodeHWList(qnLicensePool->allLocalHardwareIds())));
    }

    query = QUrlQuery(d->request.requestLine.url.query());
    bool fail = query.hasQueryItem("canceled") || !QnTransactionTransport::tryAcquireConnected(remoteGuid, false);
    d->chunkedMode = true;
    sendResponse("HTTP", fail ? CODE_INVALID_PARAMETER : CODE_OK, "application/octet-stream");
    if (fail) {
        QnTransactionTransport::connectingCanceled(remoteGuid, false);
    }
    else {
        QnTransactionMessageBus::instance()->gotConnectionFromRemotePeer(d->socket, remotePeer, QnTransactionTransport::decodeHWList(remoteHwList));
        d->socket.clear();
    }
}


}
