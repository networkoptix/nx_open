#include "audio_transmission_rest_handler.h"
#include "streaming/audio_streamer_pool.h"
#include "utils/network/http/httptypes.h"

const QString QnAudioTransmissionRestHandler::kClientIdParamName("clientId");
const QString QnAudioTransmissionRestHandler::kResourceIdParamName("resourceId");
const QString QnAudioTransmissionRestHandler::kActionParamName("action");
const QString QnAudioTransmissionRestHandler::kStartStreamAction("start");
const QString QnAudioTransmissionRestHandler::kStopStreamAction("stop");

int QnAudioTransmissionRestHandler::executeGet(
    const QString &path,
    const QnRequestParamList &params,
    QByteArray &result,
    QByteArray &contentType,
    const QnRestConnectionProcessor *)
{
    QN_UNUSED(path);
    QN_UNUSED(result);
    QN_UNUSED(contentType);

    if(!validateParams(params))
        return nx_http::StatusCode::badRequest;

    auto clientId = params.find(kClientIdParamName)->second;
    auto resourceId = params.find(kResourceIdParamName)->second;
    auto action = params.find(kActionParamName)->second;

    bool res = false;
    if(action == kStartStreamAction)
        res = QnAudioStreamerPool::instance()->startStreamToResource(clientId, resourceId);
    else
        res = QnAudioStreamerPool::instance()->stopStreamToResource(clientId, resourceId);


    if(res)
        return nx_http::StatusCode::ok;
    else
        return nx_http::StatusCode::internalServerError;

}

bool QnAudioTransmissionRestHandler::validateParams(const QnRequestParamList &params) const
{    
    bool hasAllParams = params.contains(kClientIdParamName)
        && params.contains(kResourceIdParamName)
        && params.contains(kActionParamName);

    auto action = params.find(kActionParamName)->second;

    return hasAllParams && (action == kStartStreamAction || action == kStopStreamAction);

}
