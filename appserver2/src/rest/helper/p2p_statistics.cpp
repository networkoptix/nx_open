#include "p2p_statistics.h"

#include <nx/network/system_socket.h>
#include <common/common_module.h>
#include <transaction/transaction.h>
#include <nx/p2p/p2p_connection.h>
#include "ec2_connection.h"

namespace rest {
namespace helper {

QByteArray P2pStatistics::kUrlPath("api/p2pStats");

ec2::ApiP2pStatisticsData P2pStatistics::data(QnCommonModule* commonModule)
{
    ec2::ApiP2pStatisticsData result;

    result.totalBytesSent = nx::network::totalSocketBytesSent();

    ec2::Ec2DirectConnection* connection =
        dynamic_cast<ec2::Ec2DirectConnection*> (commonModule->ec2Connection().get());
    if (connection)
    {
        ec2::detail::QnDbManager* db = connection->messageBus()->getDb();
        ec2::ApiTransactionDataList tranList;
        db->doQuery(ec2::ApiTranLogFilter(), tranList);
        for (const auto& tran : tranList)
            result.totalDbData += tran.dataSize;
    }

    const auto& counters = nx::p2p::Connection::sendCounters();
    qint64 webSocketBytes = 0;
    for (int i = 0; i < (int) nx::p2p::MessageType::counter; ++i)
        result.p2pCounters.insert(toString(nx::p2p::MessageType(i)),  counters[i]);

    return result;
}

} // helper
} // rest
