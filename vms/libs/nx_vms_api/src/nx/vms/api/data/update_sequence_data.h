#pragma once

#include "sync_marker_record_data.h"

#include <vector>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API UpdateSequenceData: Data
{
    std::vector<SyncMarkerRecordData> markers;
};
#define UpdateSequenceData_Fields (markers)

} // namespace api
} // namespace vms
} // namespace nx
