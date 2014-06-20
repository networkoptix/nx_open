#ifndef __SERVER_ALIVE_DATA_H_
#define __SERVER_ALIVE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiPeerAliveData: ApiData
    {
        QnId peerId;
        int peerType; //TODO: #Elric what should we do to serialize it properly?
        bool isAlive;
        QList<QByteArray> hardwareIds;
    };
#define ApiPeerAliveData_Fields (peerId)(peerType)(isAlive)(hardwareIds)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiPeerAliveData);

#endif // __SERVER_ALIVE_DATA_H_
