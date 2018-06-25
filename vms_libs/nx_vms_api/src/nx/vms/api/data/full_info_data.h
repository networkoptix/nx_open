#pragma once

#include "data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API FullInfoData: Data
{
    ResourceTypeDataList resourceTypes;
    MediaServerDataList servers;
    MediaServerUserAttributesDataList serversUserAttributesList;
    CameraDataList cameras;
    CameraAttributesDataList cameraUserAttributesList;
    UserDataList users;
    UserRoleDataList userRoles;
    AccessRightsDataList accessRights;
    LayoutDataList layouts;
    VideowallDataList videowalls;
    EventRuleDataList rules;
    ServerFootageDataList cameraHistory;
    LicenseDataList licenses;
    DiscoveryDataList discoveryData;
    ResourceParamWithRefDataList allProperties;
    StorageDataList storages;
    ResourceStatusDataList resStatusList;
    WebPageDataList webPages;
    LayoutTourDataList layoutTours;
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
    (cameraHistory) \
    (licenses) \
    (discoveryData) \
    (allProperties) \
    (storages) \
    (resStatusList) \
    (webPages) \
    (accessRights) \
    (userRoles) \
    (layoutTours)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::FullInfoData)
