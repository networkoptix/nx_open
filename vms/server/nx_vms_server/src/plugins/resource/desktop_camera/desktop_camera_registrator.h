#pragma once

#if defined(ENABLE_DESKTOP_CAMERA)

#include <network/tcp_connection_processor.h>
#include <nx/mediaserver/server_module_aware.h>

class QnDesktopCameraRegistratorPrivate;

class QnDesktopCameraRegistrator:
    public nx::mediaserver::ServerModuleAware,
    public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraRegistrator(
        QnMediaServerModule* serverModule,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner);

protected:
    virtual void run() override;
};

#endif // defined(ENABLE_DESKTOP_CAMERA)
