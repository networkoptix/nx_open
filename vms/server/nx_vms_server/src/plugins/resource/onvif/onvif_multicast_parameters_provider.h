#pragma once

#include <nx/vms/server/resource/camera.h>
#include <plugins/resource/onvif/onvif_multicast_parameters_proxy.h>

class QnPlOnvifResource;

namespace nx::vms::server::resource {

class OnvifMulticastParametersProvider: public Camera::AdvancedParametersProvider
{
public:
    OnvifMulticastParametersProvider(
        QnPlOnvifResource* resource,
        nx::vms::api::StreamIndex streamIndex);

    virtual QnCameraAdvancedParams descriptions() override;

    virtual QnCameraAdvancedParamValueMap get(const QSet<QString>& ids) override;

    virtual QSet<QString> set(const QnCameraAdvancedParamValueMap& values) override;

    MulticastParameters getMulticastParameters();

    bool setMulticastParameters(MulticastParameters multicastParameters);

private:
    QnPlOnvifResource* m_resource = nullptr;
    nx::vms::api::StreamIndex m_streamIndex = nx::vms::api::StreamIndex::undefined;
};

} // namespace nx::vms::server::resource
