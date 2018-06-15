#pragma once

#include "data.h"

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PeerSystemTimeData: public Data
{
    QnUuid peerID;
    //!Serialized \a ec2::TimePriorityKey structure
    qint64 timePriorityKey = 0;
    //!UTC, millis from epoch
    qint64 peerSysTime = 0;
};
#define PeerSystemTimeData_Fields (peerID)(timePriorityKey)(peerSysTime)

struct NX_VMS_API PeerSyncTimeData: public Data
{
    qint64 syncTimeMs = 0;
};
#define PeerSyncTimeData_Fields (syncTimeMs)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::PeerSystemTimeData)
Q_DECLARE_METATYPE(nx::vms::api::PeerSyncTimeData)
