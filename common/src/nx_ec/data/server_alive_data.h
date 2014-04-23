#ifndef __SERVER_ALIVE_DATA_H_
#define __SERVER_ALIVE_DATA_H_

#include "nx_ec/ec_api.h"
#include "api_data.h"
#include "utils/common/id.h"

namespace ec2
{
    struct ApiServerAliveData: public ApiData
    {
        QnId serverId;
        bool isAlive;
        bool isClient;
    };

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiServerAliveData, (serverId) (isAlive)(isClient) )
    QN_FUSION_DECLARE_FUNCTIONS(ApiServerAliveData, (binary))
}

#endif // __SERVER_ALIVE_DATA_H_
