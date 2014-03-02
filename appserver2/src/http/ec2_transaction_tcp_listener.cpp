#include "ec2_transaction_tcp_listener.h"
#include "utils/network/tcp_connection_priv.h"
#include "transaction/transaction_message_bus.h"
#include "nx_ec/data/ec2_full_data.h"
#include "database/db_manager.h"
#include "common/common_module.h"

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

    d->chunkedMode = true;
    d->response.headers.insert(nx_http::HttpHeader("guid", qnCommon->moduleGUID().toByteArray()));
    d->response.headers.insert(nx_http::HttpHeader("time", QByteArray::number(localTime)));

    if (removeGuid.isNull()) {
        qWarning() << "Invalid incoming request. GUID must be filled!";
        sendResponse("HTTP", CODE_INVALID_PARAMETER, "application/octet-stream");
        return;
    }

    sendResponse("HTTP", CODE_OK, "application/octet-stream");

    QnTransactionMessageBus::instance()->gotConnectionFromRemotePeer(d->socket, isClient, removeGuid, timeDiff);
    d->socket.clear();
}

}
