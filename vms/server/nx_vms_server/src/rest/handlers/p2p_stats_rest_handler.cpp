#include "p2p_stats_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include <rest/helper/p2p_statistics.h>

int QnP2pStatsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    result.setReply(rest::helper::P2pStatistics::data(owner->commonModule()));
    return nx::network::http::StatusCode::ok;
}
