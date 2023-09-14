// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

namespace nx::vms::api {

namespace rules {

struct EventInfo;

struct Rule;
using RuleList = std::vector<Rule>;

} // namespace rules

enum class PeerType;
enum class ResourceStatus;

struct AnalyticsEngineData;
struct AnalyticsPluginData;
struct CameraData;
struct CameraAttributesData;
struct DiscoveredServerData;
struct DiscoveryData;
struct EventRuleData;
struct FullInfoData;
struct HardwareIdMapping;
struct IdData;
struct LayoutData;
struct ShowreelData;
struct LicenseData;
struct MediaServerData;
struct MediaServerUserAttributesData;
struct ResourceParamWithRefData;
struct ResourceStatusData;
struct ResourceTypeData;
struct RuntimeData;
struct ServerFootageData;
struct ServerRuntimeEventData;
struct StorageData;
struct UserData;
struct UserGroupData;
struct VideowallControlMessageData;
struct VideowallData;
struct WebPageData;

using AnalyticsEngineDataList = std::vector<AnalyticsEngineData>;
using AnalyticsPluginDataList = std::vector<AnalyticsPluginData>;
using CameraDataList = std::vector<CameraData>;
using CameraAttributesDataList = std::vector<CameraAttributesData>;
using DiscoveredServerDataList = std::vector<DiscoveredServerData>;
using DiscoveryDataList = std::vector<DiscoveryData>;
using EventRuleDataList = std::vector<EventRuleData>;
using IdDataList = std::vector<IdData>;
using LayoutDataList = std::vector<LayoutData>;
using ShowreelDataList = std::vector<ShowreelData>;
using LicenseDataList = std::vector<LicenseData>;
using MediaServerDataList = std::vector<MediaServerData>;
using MediaServerUserAttributesDataList = std::vector<MediaServerUserAttributesData>;
using ResourceParamWithRefDataList = std::vector<ResourceParamWithRefData>;
using ResourceStatusDataList = std::vector<ResourceStatusData>;
using ResourceTypeDataList = std::vector<ResourceTypeData>;
using ServerFootageDataList = std::vector<ServerFootageData>;
using StorageDataList = std::vector<StorageData>;
using UserDataList = std::vector<UserData>;
using UserGroupDataList = std::vector<UserGroupData>;
using VideowallDataList = std::vector<VideowallData>;
using WebPageDataList = std::vector<WebPageData>;

} // namespace nx::vms::api
