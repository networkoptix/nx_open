#include "hikvision_request_helper.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace vms::server {
namespace plugins {
namespace hikvision {

static const auto kIntegratePath = "ISAPI/System/Network/Integrate";
static const auto kDeviceInfoPath = "ISAPI/System/deviceInfo";
static const auto kEnableProtocolsXmlTemplate = QString::fromUtf8(R"xml(
<?xml version:"1.0" encoding="UTF-8"?>
<Integrate>
    %1
</Integrate>
)xml").trimmed();

static const auto kSetOnvifUserPath = lit("ISAPI/Security/ONVIF/users/");
static const auto kSetOnvifUserXml = QString::fromUtf8(R"xml(
<?xml version:"1.0" encoding="UTF-8"?>
<User>
    <id>%1</id>
    <userName>%2</userName>
    <password>%3</password>
    <userType>administrator</userType>
</User>
)xml").trimmed();

static const std::map<Protocol, QString> kHikvisionIntegrationProtocols = {
    {Protocol::onvif, lit("<ONVIF><enable>true</enable><certificateType/></ONVIF>")},
    {Protocol::isapi, lit("<ISAPI><enable>true</enable></ISAPI>")},
    {Protocol::cgi, lit("<CGI><enable>true</enable><certificateType/></CGI>")}
};

ProtocolStates IsapiRequestHelper::fetchIntegrationProtocolInfo()
{
    ProtocolStates integrationProtocolStates;
    const auto document = get(kIntegratePath);
    if (!document)
        return integrationProtocolStates;

    for (const auto& entry: kHikvisionIntegrationProtocols)
    {
        const auto& protocol = entry.first;
        const auto protocolElement = document->child(QnLexical::serialized(protocol));
        ProtocolState state;
        if (!protocolElement)
        {
            state.supported = true;
            state.enabled = protocolElement->booleanOrFalse(lit("enable"));
        }

        integrationProtocolStates.emplace(protocol, state);
    }

    return integrationProtocolStates;
}

bool IsapiRequestHelper::enableIntegrationProtocols(const ProtocolStates& integrationProtocolStates)
{
    QString enableProtocolsXmlString;
    for (const auto& protocolState: integrationProtocolStates)
    {
        if (!protocolState.second.supported || protocolState.second.enabled)
            continue;

        const auto itr = kHikvisionIntegrationProtocols.find(protocolState.first);
        NX_ASSERT(itr != kHikvisionIntegrationProtocols.cend());
        if (itr == kHikvisionIntegrationProtocols.cend())
            continue;

        enableProtocolsXmlString.append(itr->second);
    }

    if (enableProtocolsXmlString.isEmpty())
        return true;

    const auto result = put(
        kIntegratePath,
        kEnableProtocolsXmlTemplate.arg(enableProtocolsXmlString));

    NX_DEBUG(this, "Enable integration protocols result: %1", result);
    return result;
}

bool IsapiRequestHelper::checkIsapiSupport()
{
    return (bool) get(kDeviceInfoPath);
}

std::optional<std::map<int, QString>> IsapiRequestHelper::getOnvifUsers()
{
    const auto document = get(kSetOnvifUserPath);
    if (!document)
        return std::nullopt;

    std::map<int, QString> users;
    for (const auto& user: document->children(lit("User")))
    {
        const auto id = user.integer(lit("id"));
        const auto name = user.string(lit("userName"));
        if (!id || !name)
            return std::nullopt;

        users.emplace(*id, *name);
    }

    NX_VERBOSE(this, "ONVIF users %1", containerString(users));
    return users;
}

bool IsapiRequestHelper::setOnvifCredentials(int userId, const QString& login, const QString& password)
{
    const auto userNumber = QString::number(userId);
    const auto result = put(
        kSetOnvifUserPath + userNumber,
        kSetOnvifUserXml.arg(userNumber, login, password));

    NX_DEBUG(this, "Set ONVIF credentials result: %1", result);
    return result;
}

} // namespace hikvision
} // namespace plugins
} // namespace vms::server
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::plugins::hikvision, Protocol,
    (nx::vms::server::plugins::hikvision::Protocol::onvif, "ONVIF")
    (nx::vms::server::plugins::hikvision::Protocol::isapi, "ISAPI")
    (nx::vms::server::plugins::hikvision::Protocol::cgi, "CGI")
)

