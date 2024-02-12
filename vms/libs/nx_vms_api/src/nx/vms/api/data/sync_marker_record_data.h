// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API SyncMarkerRecordData
{
    nx::Uuid peerID;
    nx::Uuid dbID;
    int sequence = 0;
};
#define SyncMarkerRecordData_Fields (peerID)(dbID)(sequence)
NX_VMS_API_DECLARE_STRUCT(SyncMarkerRecordData)

} // namespace api
} // namespace vms
} // namespace nx
