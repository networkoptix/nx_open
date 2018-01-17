#include "audit_log_rest_handler.h"

#include <common/common_module.h>
#include <nx/network/http/http_types.h>
#include "api/model/audit/audit_record.h"
#include <database/server_db.h>
#include "recording/time_period.h"
#include "rest/server/json_rest_result.h"
#include "core/resource_management/resource_pool.h"
#include "recorder/storage_manager.h"
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/string.h>

int QnAuditLogRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& contentBody,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);

    QnUuid sessionId = QnUuid(params.value("sessionId"));
    QnTimePeriod period;
    period.startTimeMs = nx::utils::parseDateTime(params.value("from")) / 1000;
    period.durationMs = -1;
    if (params.contains("to")) {
        qint64 endTimeMs = nx::utils::parseDateTime(params.value("to")) / 1000;
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


    QnAuditRecordList outputData = qnServerDb->getAuditData(period, sessionId);
    for(QnAuditRecord& record: outputData)
    {
        if (record.isPlaybackType()) {
            QnLatin1Array playbackFlags;
            for (const auto& id: record.resources)
            {
                QnResourcePtr res = owner->resourcePool()->getResourceById(id);
                QnTimePeriod period;
                period.startTimeMs = record.rangeStartSec * 1000ll;
                period.durationMs = (record.rangeEndSec - record.rangeStartSec) * 1000ll;
                bool exists = res && QnStorageManager::isArchiveTimeExists(res->getUniqueId(), period);
                playbackFlags.append(exists ? '1' : '0');
            }
            record.addParam("archiveExist", playbackFlags);
        }
    }

    QnFusionRestHandlerDetail::serializeRestReply(outputData, params, contentBody, contentType, QnRestResult());
    return nx::network::http::StatusCode::ok;
}
