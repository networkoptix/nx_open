#include "get_hardware_ids_rest_handler.h"

#include <llutil/hardware_id.h>
#include <nx/network/http/http_types.h>

int QnGetHardwareIdsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    const QStringList hardwareIds = LLUtil::getAllHardwareIds();
    result.setReply(hardwareIds);
    return nx::network::http::StatusCode::ok;
}
