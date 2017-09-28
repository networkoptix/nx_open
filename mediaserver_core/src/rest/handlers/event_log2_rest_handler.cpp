#include "event_log2_rest_handler.h"

#include <api/helpers/event_log_request_data.h>

#include <database/server_db.h>

#include <rest/server/json_rest_result.h>
#include <rest/server/rest_connection_processor.h>

#include <nx/vms/event/actions/abstract_action.h>

int QnEventLog2RestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& contentBody,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    QnEventLogRequestData request;
    request.loadFromParams(owner->resourcePool(), params);

    QString errStr;
    QnRestResult restResult;
    nx::vms::event::ActionDataList outputData;
    if (request.isValid(&errStr))
    {
        outputData = qnServerDb->getActions(request);
    }
    else
    {
        restResult.setError(QnRestResult::InvalidParameter, errStr);
    }

    QnFusionRestHandlerDetail::serializeRestReply(
        outputData, params, contentBody, contentType, restResult);

    return nx_http::StatusCode::ok;
}
