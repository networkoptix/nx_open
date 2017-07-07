#pragma once

#include <QtGlobal>

#include "api_data.h"
#include "api_fwd.h"


namespace ec2 {

struct ApiPeerSystemTimeData: public ApiData
{
    QnUuid peerID;
    //!Serialized \a ec2::TimePriorityKey structure
    qint64 timePriorityKey;
    //!UTC, millis from epoch
    qint64 peerSysTime;

    ApiPeerSystemTimeData() : timePriorityKey(0), peerSysTime(0) {}
};
#define ApiPeerSystemTimeData_Fields (peerID)(timePriorityKey)(peerSysTime)

struct ApiPeerSyncTimeData: public ApiData
{
    qint64 syncTimeMs = 0;
};
#define ApiPeerSyncTimeData_Fields (syncTimeMs)

} // namespace ec2
