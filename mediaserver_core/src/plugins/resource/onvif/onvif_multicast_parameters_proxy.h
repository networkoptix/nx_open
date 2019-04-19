#pragma once

#include <optional>
#include <string>
#include <QtCore/QString>

struct QnCameraAdvancedParams;
class QnPlOnvifResource;

namespace nx {
namespace vms {
namespace server {
namespace resource {

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
        Qn::StreamIndex streamIndex);

    MulticastParameters multicastParameters();

    bool setMulticastParameters(MulticastParameters parameters);

private:
    bool setAudioEncoderMulticastParameters(MulticastParameters& parameters);

private:
    QnPlOnvifResource* m_resource = nullptr;
    Qn::StreamIndex m_streamIndex{ Qn::StreamIndex::undefined };
};

} // namespace resource
} // namespace server
} // namespace vms
} // namespace nx
