#pragma once

#include <optional>

struct QnCameraAdvancedParams;
class QnPlOnvifResource;

namespace nx::vms::server::resource {

struct MulticastParameters
{
    std::optional<std::string> address;
    std::optional<int> port;
    std::optional<int> ttl;
};

class OnvifMulticastParametersProxy
{
public:
    OnvifMulticastParametersProxy(
        QnPlOnvifResource* onvifResource,
        nx::vms::api::StreamIndex streamIndex);

    MulticastParameters multicastParameters();

    bool setMulticastParameters(const MulticastParameters parameters);

private:
    QnPlOnvifResource* m_resource = nullptr;
    nx::vms::api::StreamIndex m_streamIndex;
};

} // namespace nx::vms::server
