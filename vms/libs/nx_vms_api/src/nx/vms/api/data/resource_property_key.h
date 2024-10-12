// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::api {

namespace server_properties {

NX_VMS_API extern const QString kCpuArchitecture;
NX_VMS_API extern const QString kCpuModelName;
NX_VMS_API extern const QString kFlavor;
NX_VMS_API extern const QString kOsInfo;
NX_VMS_API extern const QString kPhysicalMemory;
NX_VMS_API extern const QString kGuidConflictDetected;
// TODO: #rvasilenko Can we change the property text safely?
NX_VMS_API extern const QString kBrand;
NX_VMS_API extern const QString kFullVersion;
NX_VMS_API extern const QString kPublicationType;
NX_VMS_API extern const QString kPublicIp;
NX_VMS_API extern const QString kSystemRuntime;
NX_VMS_API extern const QString kNetworkInterfaces;
NX_VMS_API extern const QString kUdtInternetTraffic_bytes;
NX_VMS_API extern const QString kHddList;
NX_VMS_API extern const QString kNvrPoePortPoweringModes;
NX_VMS_API extern const QString kCertificate;
NX_VMS_API extern const QString kUserProvidedCertificate;
NX_VMS_API extern const QString kWebCamerasDiscoveryEnabled;
NX_VMS_API extern const QString kHardwareDecodingEnabled;
NX_VMS_API extern const QString kMetadataStorageIdKey;
NX_VMS_API extern const QString kTimeZoneInformation;
NX_VMS_API extern const QString kPortForwardingConfigurations;

} // namespace server_properties

namespace user_properties {

NX_VMS_API extern const QString kUserSettings;

} // namespace server_properties

} // namespace nx::vms::api
