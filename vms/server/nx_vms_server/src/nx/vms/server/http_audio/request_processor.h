#pragma once

#include <network/tcp_connection_processor.h>
#include <network/tcp_listener.h>
#include <nx/network/websocket/websocket.h>
#include <nx/vms/server/server_module_aware.h>
#include <camera/camera_fwd.h>

namespace nx::vms::server::http_audio {

class AudioRequestProcessor:
    public QnTCPConnectionProcessor,
    public ServerModuleAware
{
public:
    AudioRequestProcessor(
        QnMediaServerModule* serverModule,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner);

    virtual ~AudioRequestProcessor();

protected:
    virtual void run() override;

private:
    QnVideoCameraPtr getVideoCamera(const QString& cameraId);
    void startAudioStreaming(
        nx::network::aio::AsyncChannelPtr socket, const QnVideoCameraPtr& camera);
};

} // namespace nx::vms::server::http_audio
