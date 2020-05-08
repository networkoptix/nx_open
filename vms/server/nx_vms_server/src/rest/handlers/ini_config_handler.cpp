#include "ini_config_handler.h"

#include <nx/kit/ini_config.h>
#include <nx/kit/json.h>

#include <nx/network/http/http_types.h>

int QnIniConfigHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& responseMessageBody,
    QByteArray& contentType,
    const QnRestConnectionProcessor* /*owner*/)
{
    namespace StatusCode = nx::network::http::StatusCode;

    const nx::kit::Json replyJson = nx::kit::Json::object{
        {"iniFilesDir", nx::kit::IniConfig::iniFilesDir()},
    };

    contentType = "application/json";

    responseMessageBody = QByteArray::fromStdString(replyJson.dump());

    return StatusCode::ok;
}
