#pragma once

#include <QtGlobal>

#include "api_data.h"
#include "api_fwd.h"


namespace ec2 {

struct ApiPeerSystemTimeData: public nx::vms::api::Data
{
    QnUuid peerID;
    //!Serialized \a ec2::TimePriorityKey structure
    qint64 timePriorityKey = 0;
    //!UTC, millis from epoch
    qint64 peerSysTime = 0;
};
#define ApiPeerSystemTimeData_Fields (peerID)(timePriorityKey)(peerSysTime)

struct ApiPeerSyncTimeData: public nx::vms::api::Data
{
    qint64 syncTimeMs = 0;
};
#define ApiPeerSyncTimeData_Fields (syncTimeMs)

} // namespace ec2
