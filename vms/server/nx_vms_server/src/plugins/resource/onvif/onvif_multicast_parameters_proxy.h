#pragma once

#include <optional>
#include <string>
#include <QString>

#include <nx/vms/server/resource/multicast_parameters.h>

struct QnCameraAdvancedParams;
class QnPlOnvifResource;

namespace nx::vms::server::resource {

class OnvifMulticastParametersProxy
{
public:
    OnvifMulticastParametersProxy(
        QnPlOnvifResource* onvifResource,
        nx::vms::api::StreamIndex streamIndex);

    MulticastParameters multicastParameters();

    bool setMulticastParameters(MulticastParameters parameters);

private:
    bool setVideoEncoderMulticastParameters(MulticastParameters& parameters);
    bool setAudioEncoderMulticastParameters(MulticastParameters& parameters);

private:
    QnPlOnvifResource* m_resource = nullptr;
    nx::vms::api::StreamIndex m_streamIndex;
};

} // namespace nx::vms::server
