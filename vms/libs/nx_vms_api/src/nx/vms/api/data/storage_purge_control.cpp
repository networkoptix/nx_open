// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_purge_control.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StoragePurgeControlData, (json), StoragePurgeControlData_Fields)

QString StoragePurgeControlData::toString() const
{
    return QJson::serialized(*this);
}

} // namespace nx::vms::api
