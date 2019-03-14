#pragma once

#include <plugins/utils/xml_request_helper.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms::server {
namespace plugins {
namespace hikvision {

enum class Protocol
{
    onvif,
    isapi,
    cgi,
};

struct ProtocolState
{
    bool supported = false;
    bool enabled = false;
};

using ProtocolStates = std::map<Protocol, ProtocolState>;

class IsapiRequestHelper: protected nx::plugins::utils::XmlRequestHelper
{
public:
    using XmlRequestHelper::XmlRequestHelper;

    ProtocolStates fetchIntegrationProtocolInfo();
    bool enableIntegrationProtocols(const ProtocolStates& integrationProtocolStates);

    bool checkIsapiSupport();
    std::optional<std::map<int, QString>> getOnvifUsers();
    bool setOnvifCredentials(int userId, const QString& login, const QString& password);
};

} // namespace hikvision
} // namespace plugins
} // namespace vms::server
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::server::plugins::hikvision::Protocol, (metatype)(lexical))
