#pragma once

#include <optional>

struct QnCameraAdvancedParams;
class QnPlOnvifResource;
class QString;

namespace nx::vms::server::resource {

struct MulticastParameters
{
    std::optional<std::string> address;
    std::optional<int> port;
    std::optional<int> ttl;

    QString toString() const;
};

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
