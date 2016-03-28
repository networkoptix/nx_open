#ifndef __EC2_FULL_DATA_H_
#define __EC2_FULL_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiFullInfoData: ApiData {
        ApiResourceTypeDataList resourceTypes;
        ApiMediaServerDataList servers;
        ApiMediaServerUserAttributesDataList serversUserAttributesList;
        ApiCameraDataList cameras;
        ApiCameraAttributesDataList cameraUserAttributesList;
        ApiUserDataList users;
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
    };
#define ApiFullInfoData_Fields (resourceTypes)(servers)(serversUserAttributesList)(cameras)(cameraUserAttributesList)(users)(layouts)(videowalls)(rules)\
                               (cameraHistory)(licenses)(discoveryData)(allProperties)(storages)(resStatusList)(webPages)(accessRights)

} // namespace ec2

#endif // __EC2_FULL_DATA_H_

