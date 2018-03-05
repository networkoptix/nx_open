#include "basic_cloud_module_url_fetcher.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>

namespace nx {
namespace network {
namespace cloud {

const char* const kCloudDbModuleName = "cdb";
const char* const kConnectionMediatorModuleName = "hpm";
const char* const kNotificationModuleName = "notification_module";

//-------------------------------------------------------------------------------------------------
// class CloudInstanceSelectionAttributeNameset

CloudInstanceSelectionAttributeNameset::CloudInstanceSelectionAttributeNameset()
{
    registerResource(cloudInstanceName, "cloud.instance.name", QVariant::String);
    registerResource(vmsVersionMajor, "vms.version.major", QVariant::Int);
    registerResource(vmsVersionMinor, "vms.version.minor", QVariant::Int);
    registerResource(vmsVersionBugfix, "vms.version.bugfix", QVariant::Int);
    registerResource(vmsVersionBuild, "vms.version.build", QVariant::Int);
    registerResource(vmsVersionFull, "vms.version.full", QVariant::String);
    registerResource(vmsBeta, "vms.beta", QVariant::String);
    registerResource(vmsCustomization, "vms.customization", QVariant::String);
    registerResource(cdbUrl, kCloudDbModuleName, QVariant::String);

    registerResource(hpmUrl, kConnectionMediatorModuleName, QVariant::String);
    registerResource(hpmTcpUrl, "hpm.tcpUrl", QVariant::String);
    registerResource(hpmUdpUrl, "hpm.udpUrl", QVariant::String);

    registerResource(notificationModuleUrl, kNotificationModuleName, QVariant::String);
}

//-------------------------------------------------------------------------------------------------

VeryBasicCloudModuleUrlFetcher::VeryBasicCloudModuleUrlFetcher():
    m_modulesXmlUrl(AppInfo::defaultCloudModulesXmlUrl(nx::network::SocketGlobals::cloud().cloudHost()))
{
}

void VeryBasicCloudModuleUrlFetcher::setModulesXmlUrl(nx::utils::Url url)
{
    m_modulesXmlUrl = std::move(url);
}

} // namespace cloud
} // namespace network
} // namespace nx
