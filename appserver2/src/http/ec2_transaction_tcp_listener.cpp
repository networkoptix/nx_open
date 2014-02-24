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
    d->chunkedMode = true;
    d->response.headers.insert(nx_http::HttpHeader("guid", qnCommon->moduleGUID().toByteArray()));
    sendResponse("HTTP", CODE_OK, "application/octet-stream");

    bool needFullSync = d->request.requestLine.url.query().contains("fullsync");
    QUuid remoteGuid  = QUrlQuery(d->request.requestLine.url.query()).queryItemValue(lit("guid"));
    QnTransactionMessageBus::instance()->gotConnectionFromRemotePeer(d->socket, remoteGuid, needFullSync);
    d->socket.clear();
}

}
