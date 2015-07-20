#include "audit_log_rest_handler.h"

#include <common/common_module.h>
#include "utils/network/http/httptypes.h"
#include "api/model/audit/audit_record.h"
#include "events/events_db.h"
#include "recording/time_period.h"

int QnAuditLogRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    Q_UNUSED(result);
    Q_UNUSED(contentType);


    QnUuid sessionId = QnUuid(params.value("sessionId"));
    QnTimePeriod period;
    period.startTimeMs = params.value("startTimeMs").toLongLong();
    period.durationMs = -1;
    qint64 endTimeMs = params.value("endTimeMs").toLongLong();
    if (endTimeMs > 0)
        period.durationMs = endTimeMs - period.startTimeMs;

    QnAuditRecordList outputData = qnEventsDB->getAuditData(period, sessionId);

    QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType);
    return nx_http::StatusCode::ok;
}
