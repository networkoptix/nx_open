#include "audio_transmission_rest_handler.h"
#include "streaming/audio_streamer_pool.h"
#include "utils/network/http/httptypes.h"

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

    QString error;

    if (!validateParams(params, error))
    {
        result.setError(QnJsonRestResult::InvalidParameter, error);
        return nx_http::StatusCode::ok;
    }

    auto clientId = params[kClientIdParamName];
    auto resourceId = params[kResourceIdParamName];
    auto action = params[kActionParamName];

    bool res = false;

    if (action == kStartStreamAction)
        res = QnAudioStreamerPool::instance()->startStreamToResource(clientId, resourceId, error);
    else
        res = QnAudioStreamerPool::instance()->stopStreamToResource(clientId, resourceId, error);


    if (res)
    {
        return nx_http::StatusCode::ok;
    }
    else
    {
        result.setError(QnJsonRestResult::CantProcessRequest, error);
        return nx_http::StatusCode::ok;
    }

}

bool QnAudioTransmissionRestHandler::validateParams(const QnRequestParams &params, QString& error) const
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
