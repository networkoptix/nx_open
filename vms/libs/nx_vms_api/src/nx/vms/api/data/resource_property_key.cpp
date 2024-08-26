// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_property_key.h"

namespace nx::vms::api {

namespace server_properties {

const QString kCpuArchitecture("cpuArchitecture");
const QString kCpuModelName("cpuModelName");
const QString kFlavor("flavor");
const QString kOsInfo("osInfo");
const QString kPhysicalMemory("physicalMemory");
const QString kGuidConflictDetected("guidConflictDetected");
const QString kBrand("productNameShort");
const QString kFullVersion("fullVersion");
const QString kPublicationType("publicationType");
const QString kPublicIp("publicIp");
const QString kSystemRuntime("systemRuntime");
const QString kNetworkInterfaces("networkInterfaces");
const QString kUdtInternetTraffic_bytes("udtInternetTraffic_bytes");
const QString kHddList("hddList");
const QString kNvrPoePortPoweringModes("nvrPoePortPoweringModes");
const QString kCertificate("certificate");
const QString kUserProvidedCertificate("userProvidedCertificate");
const QString kWebCamerasDiscoveryEnabled("webCamerasDiscoveryEnabled");
const QString kMetadataStorageIdKey("metadataStorageId");
const QString kTimeZoneInformation("timeZoneInformation");
const QString kPortForwardingConfigurations("portForwardingConfigurations");

} // namespace server_properties

namespace user_properties {

const QString kUserSettings = "userSettings";

} // namespace server_properties

} // namespace nx::vms::api
