#include "audit_log_rest_handler.h"

#include <common/common_module.h>
#include "utils/network/http/httptypes.h"
#include "api/model/audit/audit_record.h"
#include "events/events_db.h"
#include "recording/time_period.h"
#include "rest/server/json_rest_result.h"
#include "core/resource_management/resource_pool.h"
#include "recorder/storage_manager.h"

int QnAuditLogRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& contentBody, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);

    QnUuid sessionId = QnUuid(params.value("sessionId"));
    QnTimePeriod period;
    period.startTimeMs = parseDateTime(params.value("from")) / 1000;
    period.durationMs = -1;
    if (params.contains("to")) {
        qint64 endTimeMs = parseDateTime(params.value("to")) / 1000;
        if (endTimeMs > 0)
            period.durationMs = endTimeMs - period.startTimeMs;
    }

    // for compatibility with previous version
    if (params.contains("startTimeMs"))
        period.startTimeMs = params.value("startTimeMs").toLongLong();
    if (params.contains("endTimeMs")) {
        qint64 endTimeMs = params.value("endTimeMs").toLongLong();
        if (endTimeMs > 0)
            period.durationMs = endTimeMs - period.startTimeMs;
    }


    QnAuditRecordList outputData = qnEventsDB->getAuditData(period, sessionId);
    for(QnAuditRecord& record: outputData)
    {
        if (record.isPlaybackType()) {
            QnLatin1Array playbackFlags;
            for (const auto& id: record.resources) 
            {
                QnResourcePtr res = qnResPool->getResourceById(id);
                QnTimePeriod period;
                period.startTimeMs = record.rangeStartSec * 1000ll;
                period.durationMs = (record.rangeEndSec - record.rangeStartSec) * 1000ll;
                // TODO: #akulikov #backup storages. Make this work for two storage managers
                bool exists = res && qnNormalStorageMan->isArchiveTimeExists(res->getUniqueId(), period);
                playbackFlags.append(exists ? '1' : '0');
            }
            record.addParam("archiveExist", playbackFlags);
        }
    }

    QnFusionRestHandlerDetail::serializeRestReply(outputData, params, contentBody, contentType, QnRestResult());
    return nx_http::StatusCode::ok;
}
