#ifndef __EC2_FULL_DATA_H_
#define __EC2_FULL_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "api_discovery_data.h"
#include "api_resource_type_data.h"
#include "api_server_info_data.h"

namespace ec2 
{
    struct ApiFullInfoData: ApiData {
        ApiResourceTypeDataList resourceTypes;
        ApiMediaServerDataList servers;
        ApiCameraDataList cameras;
        ApiUserDataList users;
        ApiLayoutDataList layouts;
        ApiVideowallDataList videowalls;
        ApiBusinessRuleDataList rules;
        ApiCameraServerItemDataList cameraHistory;
        ApiLicenseDataList licenses;
        //ApiServerInfoData serverInfo;
        ApiDiscoveryDataList discoveryData;
    };
#define ApiFullInfoData_Fields (resourceTypes)(servers)(cameras)(users)(layouts)(videowalls)(rules)(cameraHistory)(licenses)(discoveryData)

} // namespace ec2

#endif // __EC2_FULL_DATA_H_
