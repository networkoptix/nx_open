#ifndef __SERVER_ALIVE_DATA_H_
#define __SERVER_ALIVE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiServerAliveData: ApiData
    {
        QnId serverId;
        bool isAlive;
        bool isClient;
    };
#define ApiServerAliveData_Fields (serverId)(isAlive)(isClient)

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiServerAliveData, (serverId) (isAlive)(isClient) )
    ///QN_FUSION_DECLARE_FUNCTIONS(ApiServerAliveData, (binary))
}

#endif // __SERVER_ALIVE_DATA_H_
