#include "get_hardware_info_rest_handler.h"

#include <nx/network/http/http_types.h>
#include "licensing/hardware_info.h"
#include "llutil/hardware_id.h"

int QnGetHardwareInfoHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    Q_UNUSED(params);

    const QnHardwareInfo& hardwareInfo = LLUtil::getHardwareInfo();
    result.setReply(hardwareInfo);
    return nx::network::http::StatusCode::ok;
}