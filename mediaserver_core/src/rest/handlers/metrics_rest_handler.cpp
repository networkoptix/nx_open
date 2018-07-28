#include "metrics_rest_handler.h"
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx/metrics/metrics_storage.h>

int QnMetricsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* processor)
{
    const bool addDescription = !params.contains("noDescription");
    result.reply = processor->commonModule()->metrics()->toJson(addDescription);

    return nx::network::http::StatusCode::ok;
}
