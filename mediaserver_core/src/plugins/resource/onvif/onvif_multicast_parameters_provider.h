#pragma once

#include <nx/mediaserver/resource/camera.h>
#include <plugins/resource/onvif/onvif_multicast_parameters_proxy.h>

class QnPlOnvifResource;

namespace nx {
namespace vms {
namespace server {
namespace resource {

class OnvifMulticastParametersProvider:
    public nx::mediaserver::resource::Camera::AdvancedParametersProvider
{
public:
    OnvifMulticastParametersProvider(
        QnPlOnvifResource* resource,
        Qn::StreamIndex streamIndex);

    virtual QnCameraAdvancedParams descriptions() override;

    virtual QnCameraAdvancedParamValueMap get(const QSet<QString>& ids) override;

    virtual QSet<QString> set(const QnCameraAdvancedParamValueMap& values) override;

    MulticastParameters getMulticastParameters();

    bool setMulticastParameters(MulticastParameters multicastParameters);

private:
    QnPlOnvifResource* m_resource = nullptr;
    Qn::StreamIndex m_streamIndex = Qn::StreamIndex::undefined;
};

} // namespace resource
} // namespace server
} // namespace vms
} // namespace nx
