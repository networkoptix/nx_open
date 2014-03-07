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

}

QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiServerAliveData, (serverId) (isAlive) (isClient) )

#endif // __SERVER_ALIVE_DATA_H_
