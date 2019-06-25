#pragma once

#include <nx/vms/server/resource/camera.h>
#include <nx/vms/utils/metrics/resource_providers.h>

namespace nx::vms::server::metrics {

class CameraProvider:
    public utils::metrics::ResourceProvider<resource::CameraPtr>,
    public QObject
{
public:
    CameraProvider(QnResourcePool* resourcePool);
    void startMonitoring() override;

    std::optional<utils::metrics::ResourceDescription> describe(
        const resource::CameraPtr& resource) const override;

private:
    static ParameterProviders makeProviders();
    static ParameterProviderPtr makeStreamProvider(api::StreamIndex index);

private:
    QnResourcePool* m_resourcePool;
};

} // namespace nx::vms::server::metrics
