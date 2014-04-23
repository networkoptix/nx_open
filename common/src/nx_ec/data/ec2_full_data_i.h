#ifndef QN_FULL_DATA_I_H
#define QN_FULL_DATA_I_H

#include "api_data_i.h"

namespace ec2 {

struct ApiFullInfoData: ApiData {
        ApiResourceTypeDataListData resTypes;
        ApiMediaServerList servers;
        ApiCameraList cameras;
        ApiUserList users;
        ApiLayoutList layouts;
        ApiVideowallList videowalls;
        ApiBusinessRuleList rules;
        ApiCameraServerItemList cameraHistory;
        ApiLicenseList licenses;
        ServerInfo serverInfo;
    };

#define ApiFullInfoData_Fields (resTypes)(servers)(cameras)(users)(layouts)(videowalls)(rules)(cameraHistory)(licenses)(serverInfo)

} // namespace ec2

#endif // QN_FULL_DATA_I_H