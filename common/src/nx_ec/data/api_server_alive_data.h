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
        QString systemName;
        QString version;
        QString systemInformation;
        int port;
    };
#define ApiServerAliveData_Fields (serverId)(isAlive)(isClient)(hardwareIds)(systemName)(version)(systemInformation)(port)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiServerAliveData);

#endif // __SERVER_ALIVE_DATA_H_
