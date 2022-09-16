// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_scan_info.h"

#include <typeinfo>
#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QString StorageScanInfo::toString() const
{
    if (state == RebuildState::none)
        return typeid(*this).name();

    return NX_FMT("%1(state %2, path %3, progress %4, total progress %5)",
        typeid(*this), state, path, progress, totalProgress);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StorageScanInfo, (json), StorageScanInfo_Fields)

} // namespace nx::vms::api
