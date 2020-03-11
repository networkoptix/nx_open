#pragma once

#if defined(ENABLE_DESKTOP_CAMERA)

#include <network/tcp_connection_processor.h>
#include <nx/vms/server/server_module_aware.h>

class QnDesktopCameraRegistratorPrivate;

class QnDesktopCameraRegistrator:
    public /*mixin*/ nx::vms::server::ServerModuleAware,
    public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraRegistrator(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner,
        QnMediaServerModule* serverModule);

protected:
    virtual void run() override;
};

#endif // defined(ENABLE_DESKTOP_CAMERA)
