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
        QList<QByteArray> hardwareIds;
    };
#define ApiServerAliveData_Fields (serverId)(isAlive)(isClient)(hardwareIds)

} // namespace ec2

#endif // __SERVER_ALIVE_DATA_H_
