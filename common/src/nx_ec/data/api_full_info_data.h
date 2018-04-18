#pragma once

#include <nx_ec/data/api_data.h>

#include <nx/vms/api/data/data.h>

namespace ec2 {

struct ApiFullInfoData: nx::vms::api::Data
{
    nx::vms::api::ResourceTypeDataList resourceTypes;
    ApiMediaServerDataList servers;
    ApiMediaServerUserAttributesDataList serversUserAttributesList;
    nx::vms::api::CameraDataList cameras;
    nx::vms::api::CameraAttributesDataList cameraUserAttributesList;
    ApiUserDataList users;
    ApiUserRoleDataList userRoles;
    ApiAccessRightsDataList accessRights;
    nx::vms::api::LayoutDataList layouts;
    nx::vms::api::VideowallDataList videowalls;
    nx::vms::api::EventRuleDataList rules;
    nx::vms::api::ServerFootageDataList cameraHistory;
    ApiLicenseDataList licenses;
    ApiDiscoveryDataList discoveryData;
    nx::vms::api::ResourceParamWithRefDataList allProperties;
    ApiStorageDataList storages;
    nx::vms::api::ResourceStatusDataList resStatusList;
    nx::vms::api::WebPageDataList webPages;
    nx::vms::api::LayoutTourDataList layoutTours;
};
#define ApiFullInfoData_Fields (resourceTypes)(servers)(serversUserAttributesList)(cameras)(cameraUserAttributesList)(users)(layouts)(videowalls)(rules)\
                               (cameraHistory)(licenses)(discoveryData)(allProperties)(storages)(resStatusList)(webPages)(accessRights)(userRoles)\
                               (layoutTours)

} // namespace ec2

