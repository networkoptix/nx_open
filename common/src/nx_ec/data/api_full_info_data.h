#ifndef __EC2_FULL_DATA_H_
#define __EC2_FULL_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2 
{
    struct ApiFullInfoData: ApiData {
        ApiResourceTypeDataList resourceTypes;
        ApiMediaServerDataList servers;
        ApiCameraDataList cameras;
        ApiCameraAttributesDataList cameraUserAttributesList;
        ApiUserDataList users;
        ApiLayoutDataList layouts;
        ApiVideowallDataList videowalls;
        ApiBusinessRuleDataList rules;
        ApiCameraServerItemDataList cameraHistory;
        ApiLicenseDataList licenses;
        ApiDiscoveryDataList discoveryData;
        ApiResourceParamDataList allResParams;
    };
#define ApiFullInfoData_Fields (resourceTypes)(servers)(cameras)(cameraUserAttributesList)(users)(layouts)(videowalls)(rules)(cameraHistory)(licenses)(discoveryData)(allResParams)

} // namespace ec2

#endif // __EC2_FULL_DATA_H_

