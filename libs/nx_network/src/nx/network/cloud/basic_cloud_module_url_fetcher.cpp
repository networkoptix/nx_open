// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_cloud_module_url_fetcher.h"

namespace nx::network::cloud {

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
    registerResource(vmsCustomization, "vms.customization", QVariant::String);
    registerResource(cdbUrl, kCloudDbModuleName, QVariant::String);

    registerResource(hpmUrl, kConnectionMediatorModuleName, QVariant::String);
    registerResource(hpmTcpUrl, kConnectionMediatorTcpUrlName, QVariant::String);
    registerResource(hpmUdpUrl, kConnectionMediatorUdpUrlName, QVariant::String);

    registerResource(notificationModuleUrl, kNotificationModuleName, QVariant::String);
    registerResource(speedTestModuleUrl, kSpeedTestModuleName, QVariant::String);
}

} // namespace nx::network::cloud
