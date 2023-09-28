// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../rules/rule.h"
#include "analytics_data.h"
#include "camera_attributes_data.h"
#include "camera_data.h"
#include "camera_history_data.h"
#include "discovery_data.h"
#include "event_rule_data.h"
#include "layout_data.h"
#include "license_data.h"
#include "media_server_data.h"
#include "resource_type_data.h"
#include "showreel_data.h"
#include "user_data.h"
#include "user_group_data.h"
#include "videowall_data.h"
#include "webpage_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API FullInfoData
{
    static DeprecatedFieldNames* getDeprecatedFieldNames();

    ResourceTypeDataList resourceTypes;
    MediaServerDataList servers;
    MediaServerUserAttributesDataList serversUserAttributesList;
    CameraDataList cameras;
    CameraAttributesDataList cameraUserAttributesList;
    UserDataList users;
    UserGroupDataList userGroups;
    LayoutDataList layouts;
    VideowallDataList videowalls;
    EventRuleDataList rules;
    rules::RuleList vmsRules;
    ServerFootageDataList cameraHistory;
    LicenseDataList licenses;
    DiscoveryDataList discoveryData;
    ResourceParamWithRefDataList allProperties;
    StorageDataList storages;
    ResourceStatusDataList resStatusList;
    WebPageDataList webPages;
    ShowreelDataList showreels;
    AnalyticsPluginDataList analyticsPlugins;
    AnalyticsEngineDataList analyticsEngines;
};
#define FullInfoData_Fields \
    (resourceTypes) \
    (servers) \
    (serversUserAttributesList) \
    (cameras) \
    (cameraUserAttributesList) \
    (users) \
    (layouts) \
    (videowalls) \
    (rules) \
    (vmsRules) \
    (cameraHistory) \
    (licenses) \
    (discoveryData) \
    (allProperties) \
    (storages) \
    (resStatusList) \
    (webPages) \
    (userGroups) \
    (showreels) \
    (analyticsPlugins) \
    (analyticsEngines)
NX_VMS_API_DECLARE_STRUCT_EX(FullInfoData, (ubjson)(json)(xml)(csv_record))

} // namespace api
} // namespace vms
} // namespace nx
