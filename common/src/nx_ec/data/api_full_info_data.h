#ifndef __EC2_FULL_DATA_H_
#define __EC2_FULL_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2 
{
    struct ApiFullInfoData: ApiData {
        ApiResourceTypeDataList resTypes;
        ApiMediaServerDataList servers;
        ApiCameraDataList cameras;
        ApiUserDataList users;
        ApiLayoutDataList layouts;
        ApiVideowallDataList videowalls;
        ApiBusinessRuleDataList rules;
        ApiCameraServerItemDataList cameraHistory;
        ApiLicenseDataList licenses;
        ApiServerInfoData serverInfo;
    };
#define ApiFullInfoData_Fields (resTypes)(servers)(cameras)(users)(layouts)(videowalls)(rules)(cameraHistory)(licenses)(serverInfo)


/*
    struct ApiFullInfo: public ApiFullInfoData
    {
        void toResourceList(QnFullResourceData&, const ResourceContext&) const;
    };
*/
} // namespace ec2

#endif // __EC2_FULL_DATA_H_
