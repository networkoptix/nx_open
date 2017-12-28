
#include "recording_stats_rest_handler.h"

#include <api/model/recording_stats_reply.h>
#include <common/common_module.h>
#include <nx/network/http/http_types.h>
#include <nx/fusion/model_functions.h>
#include "recorder/storage_manager.h"
#include "api/model/recording_stats_reply.h"

int QnRecordingStatsRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)

    qint64 bitrateAnalizePeriodMs = params.value("bitrateAnalizePeriodMs").toLongLong();

    // TODO: #akulikov #backup storages. Alter this for two storage managers
    auto normalStatistics = qnNormalStorageMan->getChunkStatistics(bitrateAnalizePeriodMs);
    result.setReply(normalStatistics);
    return nx::network::http::StatusCode::ok;
}
