#pragma once

#include <nx_ec/data/api_data.h>

namespace ec2 {

struct ApiFullInfoData: ApiData
{
    ApiResourceTypeDataList resourceTypes;
    ApiMediaServerDataList servers;
    ApiMediaServerUserAttributesDataList serversUserAttributesList;
    ApiCameraDataList cameras;
    ApiCameraAttributesDataList cameraUserAttributesList;
    ApiUserDataList users;
    ApiUserRoleDataList userRoles;
    ApiAccessRightsDataList accessRights;
    ApiLayoutDataList layouts;
    ApiVideowallDataList videowalls;
    ApiBusinessRuleDataList rules;
    ApiServerFootageDataList cameraHistory;
    ApiLicenseDataList licenses;
    ApiDiscoveryDataList discoveryData;
    ApiResourceParamWithRefDataList allProperties;
    ApiStorageDataList storages;
    ApiResourceStatusDataList resStatusList;
    ApiWebPageDataList webPages;
    ApiLayoutTourDataList layoutTours;
};
#define ApiFullInfoData_Fields (resourceTypes)(servers)(serversUserAttributesList)(cameras)(cameraUserAttributesList)(users)(layouts)(videowalls)(rules)\
                               (cameraHistory)(licenses)(discoveryData)(allProperties)(storages)(resStatusList)(webPages)(accessRights)(userRoles)\
                               (layoutTours)

} // namespace ec2

