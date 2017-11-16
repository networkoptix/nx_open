
#include "audio_transmission_rest_handler.h"

#include <nx/network/http/http_types.h>
#include <streaming/audio_streamer_pool.h>

namespace
{
    const QString kClientIdParamName("clientId");
    const QString kResourceIdParamName("resourceId");
    const QString kActionParamName("action");
    const QString kStartStreamAction("start");
    const QString kStopStreamAction("stop");
}

int QnAudioTransmissionRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* )
{
    QN_UNUSED(path);

    QString errorStr;
    if (!validateParams(params, errorStr))
    {
        result.setError(QnJsonRestResult::InvalidParameter, errorStr);
        return nx_http::StatusCode::ok;
    }

    auto sourceId = params[kClientIdParamName];
    auto resourceId = params[kResourceIdParamName];
    QnAudioStreamerPool::Action action = (params[kActionParamName] == kStartStreamAction)
        ? QnAudioStreamerPool::Action::Start
        : QnAudioStreamerPool::Action::Stop;

    if (!QnAudioStreamerPool::instance()->startStopStreamToResource(
            sourceId,
            QnUuid::fromStringSafe(resourceId),
            action,
            errorStr,
			params))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errorStr);
    }
    return nx_http::StatusCode::ok;
}

bool QnAudioTransmissionRestHandler::validateParams(const QnRequestParams &params, QString& error)
{
    bool ok = true;
    if (!params.contains(kClientIdParamName))
    {
        ok = false;
        error += lit("Client ID is not specified. ");
    }
    if (!params.contains(kResourceIdParamName))
    {
        ok = false;
        error += lit("Resource ID is not specified. ");
    }
    if (!params.contains(kActionParamName))
    {
        ok = false;
        error += lit("Action is not specified. ");
        return false;
    }

    auto action = params[kActionParamName];

    if (action != kStartStreamAction && action != kStopStreamAction)
    {
        ok = false;
        error += lit("Action parameter should has value of either '%1' or '%2'")
                .arg(kStartStreamAction)
                .arg(kStopStreamAction);
    }

    return ok;

}
