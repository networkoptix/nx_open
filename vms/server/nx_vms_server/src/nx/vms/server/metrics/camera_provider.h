#pragma once

#include <nx/vms/server/resource/camera.h>
#include <nx/vms/utils/metrics/resource_providers.h>

namespace nx::vms::server::metrics {

class CameraProvider:
    public QObject,
    public ServerModuleAware,
    public utils::metrics::ResourceProvider<resource::Camera*>
{
public:
    CameraProvider(QnMediaServerModule* serverModule);
    void startMonitoring() override;

    std::optional<utils::metrics::ResourceDescription> describe(
        const Resource& resource) const override;

private:
    static ParameterProviders makeProviders();
    static ParameterProviderPtr makeStreamProvider(api::StreamIndex index);
    static ParameterProviderPtr makeStorageProvider();
};

} // namespace nx::vms::server::metrics
