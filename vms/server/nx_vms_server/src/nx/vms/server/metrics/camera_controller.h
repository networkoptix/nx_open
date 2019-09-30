#pragma once

#include <nx/vms/server/resource/camera.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

namespace nx::vms::server::metrics {

/**
 * Provides cameras, which are parrented by the current server.
 */
class CameraController:
    public QObject,
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<resource::Camera*>
{
public:
    CameraController(QnMediaServerModule* serverModule);
    void start() override;

private:
    static utils::metrics::ValueGroupProviders<Resource> makeProviders();
};

} // namespace nx::vms::server::metrics
