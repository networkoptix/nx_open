// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/reflect/instrument.h>

#include "sync_marker_record_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API UpdateSequenceData
{
    std::vector<SyncMarkerRecordData> markers;
};
#define UpdateSequenceData_Fields (markers)
NX_VMS_API_DECLARE_STRUCT(UpdateSequenceData)
NX_REFLECTION_INSTRUMENT(UpdateSequenceData, UpdateSequenceData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
