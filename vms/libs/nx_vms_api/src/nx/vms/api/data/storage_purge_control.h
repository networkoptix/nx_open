// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API StoragePurgeControlData
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Pool,
        main,
        backup
    )

    nx::Uuid id;
    Pool pool;

    const StoragePurgeControlData& getId() const { return *this; }
    QString toString() const;
};
#define StoragePurgeControlData_Fields (id)(pool)
QN_FUSION_DECLARE_FUNCTIONS(StoragePurgeControlData, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StoragePurgeControlData, StoragePurgeControlData_Fields);

} // namespace nx::vms::api
