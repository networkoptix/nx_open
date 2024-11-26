// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PeerSyncTimeData
{
    qint64 syncTimeMs = 0;
};
#define PeerSyncTimeData_Fields (syncTimeMs)
NX_VMS_API_DECLARE_STRUCT(PeerSyncTimeData)
NX_REFLECTION_INSTRUMENT(PeerSyncTimeData, PeerSyncTimeData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
