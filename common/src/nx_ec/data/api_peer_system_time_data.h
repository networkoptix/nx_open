/**********************************************************
* 07 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef API_PEER_SYSTEM_TIME_DATA_H
#define API_PEER_SYSTEM_TIME_DATA_H

#include <QtGlobal>

#include "api_data.h"
#include "api_fwd.h"


namespace ec2
{
    struct ApiPeerSystemTimeData
    :
        public ApiData
    {
        QnUuid peerID;
        //!Serialized \a ec2::TimePriorityKey structure
        qint64 timePriorityKey;
        //!UTC, millis from epoch
        qint64 peerSysTime;

        ApiPeerSystemTimeData() : timePriorityKey(0), peerSysTime(0) {}
    };

#define ApiPeerSystemTimeData_Fields (peerID)(timePriorityKey)(peerSysTime)
}

#endif  //API_PEER_SYSTEM_TIME_DATA_H
