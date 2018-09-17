#include "metrics_rest_handler.h"
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/network/system_socket.h>
#include <transaction/transaction.h>
#include <nx/p2p/p2p_connection.h>
#include "ec2_connection.h"

int QnMetricsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* processor)
{
    static QnMutex mutex;
    QnMutexLocker lock(&mutex);

    auto metrics = processor->commonModule()->metrics();
    metrics->tcpConnections().totalBytesSent() = nx::network::totalSocketBytesSent();

    ec2::Ec2DirectConnection* connection =
        dynamic_cast<ec2::Ec2DirectConnection*> (processor->commonModule()->ec2Connection().get());
    if (connection)
    {
        ec2::detail::QnDbManager* db = connection->getDb();
        ec2::ApiTransactionDataList tranList;
        db->doQuery(ec2::ApiTranLogFilter(), tranList);
        qint64 logSize = 0;
        for (const auto& tran: tranList)
            logSize += tran.dataSize;
        metrics->transactions().logSize() = logSize;
    }

    const auto& counters = nx::p2p::Connection::sendCounters();
    for (int i = 0; i < (int)nx::p2p::MessageType::counter; ++i)
    {
        auto messageName = toString(nx::p2p::MessageType(i));
        metrics->p2pCounters().dataSentByMessageType()[messageName] = counters[i];
    }


    result.reply = processor->commonModule()->metrics()->toJson(params.contains("brief"));
    return nx::network::http::StatusCode::ok;
}
