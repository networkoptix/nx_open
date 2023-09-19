// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/rebuild_state.h>

#include "map.h"

namespace nx::vms::api {

/**%apidoc Info about Storage rebuild process (if running). */
struct NX_VMS_API StorageScanInfo
{
    /**%apidoc Current process state. */
    RebuildState state = RebuildState::none;

    /**%apidoc Url of the Storage being processed. */
    QString path;

    /**%apidoc Progress of the current Storage (0.0 to 1.0). */
    qreal progress = 0.0;

    /**%apidoc Total progress of the process (0.0 to 1.0). */
    qreal totalProgress = 0.0;

    QString toString() const;

    bool operator==(const StorageScanInfo& other) const = default;
};

#define StorageScanInfo_Fields (state)(path)(progress)(totalProgress)
QN_FUSION_DECLARE_FUNCTIONS(StorageScanInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StorageScanInfo, StorageScanInfo_Fields)

using StorageScanInfoFull = Map<QString, StorageScanInfo>;
using StorageScanInfoFullMap = Map<QnUuid, StorageScanInfoFull>;

} // namespace nx::vms::api
