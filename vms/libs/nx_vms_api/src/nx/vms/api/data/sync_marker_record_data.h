#pragma once

#include "data.h"

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API SyncMarkerRecordData: Data
{
    QnUuid peerID;
    QnUuid dbID;
    int sequence = 0;
};
#define SyncMarkerRecordData_Fields (peerID)(dbID)(sequence)

} // namespace api
} // namespace vms
} // namespace nx
