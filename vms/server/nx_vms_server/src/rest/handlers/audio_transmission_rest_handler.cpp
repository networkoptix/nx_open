
#include "audio_transmission_rest_handler.h"

#include <nx/network/http/http_types.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/switch.h>

static const QString kClientIdParamName("clientId");
static const QString kResourceIdParamName("resourceId");
static const QString kActionParamName("action");
static const QString kStartStreamAction("start");
static const QString kStopStreamAction("stop");

using namespace nx::network;

namespace nx::vms::server {

AudioTransmissionRestHandler::AudioTransmissionRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

rest::Response AudioTransmissionRestHandler::executeGet(const rest::Request& request)
{
    if (!rest::ini().allowGetModifications)
        throw nx::network::rest::Exception(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

static AudioTransmissionRestHandler::Params parseRestParams(const rest::Request& request)
{
    AudioTransmissionRestHandler::Params params;
    params.sourceId = request.paramOrThrow(kClientIdParamName);
    params.resourceId = QnUuid::fromStringSafe(request.paramOrThrow(kResourceIdParamName));

    const auto actionString = request.paramOrThrow(kActionParamName);
    params.action = nx::utils::switch_(actionString,
        kStartStreamAction, []() { return QnAudioStreamerPool::Action::Start; },
        kStopStreamAction, []() { return QnAudioStreamerPool::Action::Stop; },
        nx::utils::default_,
        [&actionString] () -> QnAudioStreamerPool::Action
        {
            throw rest::Exception(rest::Result::InvalidParameter, kActionParamName, actionString);
        });

    return params;
}

rest::Response AudioTransmissionRestHandler::executePost(const rest::Request& request)
{
    const auto params = parseRestParams(request);
    QString error;
    if (audioStreamPool()->startStopStreamToResource(
            params.sourceId, params.resourceId, params.action, error, request.params()))
    {
        return rest::Response::result(rest::JsonResult());
    }

    return rest::Response::error(http::StatusCode::ok, rest::Result::CantProcessRequest, error);
}

std::optional<AudioTransmissionRestHandler::Params>
    AudioTransmissionRestHandler::parseParams(const http::Request& request)
{
    rest::Request restRequest(&request, nullptr);
    try
    {
        return parseRestParams(restRequest);
    }
    catch (rest::Exception& /*error*/)
    {
        return std::nullopt;
    }
}

} // namespace nx::vms::server
