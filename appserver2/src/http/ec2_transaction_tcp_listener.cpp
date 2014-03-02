#include "ec2_transaction_tcp_listener.h"
#include "utils/network/tcp_connection_priv.h"
#include "transaction/transaction_message_bus.h"
#include "nx_ec/data/ec2_full_data.h"
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
    QUuid removeGuid  = query.queryItemValue(lit("guid"));
    qint64 removeTime  = query.queryItemValue(lit("time")).toLongLong();
    qint64 localTime = QnTransactionLog::instance()->getRelativeTime();
    qint64 timeDiff = removeTime - localTime;

    if (removeGuid.isNull()) {
        qWarning() << "Invalid incoming request. GUID must be filled!";
        sendResponse("HTTP", CODE_INVALID_PARAMETER, "application/octet-stream");
        return;
    }

    d->response.headers.insert(nx_http::HttpHeader("guid", qnCommon->moduleGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader("time", QByteArray::number(localTime)));

    // 1-st stage

    bool isConnExist;
    bool isConnConnecting;

    /*
    QnTransactionTransport::lock();
    QnTransactionTransport::getPeerInfo(removeGuid, &isConnExist, &isConnConnecting);
    bool fail = isConnExist || (isConnConnecting && removeGuid.toRfc4122() > qnCommon->moduleGUID().toRfc4122());
    if (!fail)
        QnTransactionTransport::connectInProgress(removeGuid);
    QnTransactionTransport::unlock();
    */
    bool lockOK = QnTransactionTransport::tryAcquire(removeGuid);

    sendResponse("HTTP", lockOK ? CODE_OK : CODE_INVALID_PARAMETER , "application/octet-stream");
    if (!lockOK)
        return;

    // 2-nd stage
    if (!readRequest()) {
        QnTransactionTransport::connectCanceled(removeGuid);
        return;
    }
    parseRequest();

    query = QUrlQuery(d->request.requestLine.url.query());
    bool isCanceled = query.hasQueryItem("canceled");
    //QnTransactionTransport::getPeerInfo(removeGuid, &isConnExist, &isConnConnecting);
    //fail = isCanceled || isConnExist || (isConnConnecting && removeGuid > qnCommon->moduleGUID());
    bool fail = query.hasQueryItem("canceled");

    sendResponse("HTTP", fail ? CODE_INVALID_PARAMETER : CODE_OK, "application/octet-stream");
    if (fail) {
        QnTransactionTransport::connectCanceled(removeGuid);
        return;
    }
    QnTransactionTransport::connectEstablished(removeGuid);
    QnTransactionMessageBus::instance()->gotConnectionFromRemotePeer(d->socket, isClient, removeGuid, timeDiff);
    d->chunkedMode = true;
    d->socket.clear();
}

}
