
#include "recording_stats_rest_handler.h"

#include <api/model/recording_stats_reply.h>
#include <common/common_module.h>
#include <utils/network/http/httptypes.h>
#include <utils/common/model_functions.h>
#include "recorder/storage_manager.h"
#include "api/model/recording_stats_reply.h"

int QnRecordingStatsRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)

    qint64 bitrateAnalizePeriodMs = params.value("bitrateAnalizePeriodMs").toLongLong();

    result.setReply( qnStorageMan->getChunkStatistics(bitrateAnalizePeriodMs));
    return nx_http::StatusCode::ok;
}
