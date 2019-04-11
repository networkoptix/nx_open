#pragma once

#include <nx/network/rest/handler.h>
#include <nx/vms/server/server_module_aware.h>
#include <streaming/audio_streamer_pool.h>

namespace nx::vms::server {

class AudioTransmissionRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    AudioTransmissionRestHandler(QnMediaServerModule* serverModule);

    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

    struct Params
    {
        QString sourceId;
        QnUuid resourceId;
        QnAudioStreamerPool::Action action = QnAudioStreamerPool::Action::Start;
    };

    static std::optional<Params> parseParams(const nx::network::http::Request& request);
};

} // namespace nx::vms::server
