// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rebuild_archive_reply.h"

#include <nx/utils/log/assert.h>

namespace {

QString stateName(Qn::RebuildState state)
{
    switch (state)
    {
        case Qn::RebuildState::RebuildState_None:
            return "none";
        case Qn::RebuildState::RebuildState_PartialScan:
            return "partial";
        case Qn::RebuildState::RebuildState_FullScan:
            return "full";
        default:
            NX_ASSERT("Unknown enum value");
            return "unknown";
    }
}

} // namespace

QString QnStorageScanData::toString() const
{
    if (state == Qn::RebuildState::RebuildState_None)
        return "QnStorageScanData()";

    return nx::format("QnStorageScanData(state %1, path %2, progress %3, total progress %4)").args(
        stateName(state), path, progress, totalProgress);
}
