
#include "audit_log_rest_handler.h"

#include <common/common_module.h>
#include "utils/network/http/httptypes.h"
#include "api/model/audit/audit_record.h"

int QnAuditLogRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    Q_UNUSED(result);
    Q_UNUSED(contentType);


    QnAuditRecordList outputData;

    QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType);
    return nx_http::StatusCode::ok;
}
