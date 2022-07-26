// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>
#include <chrono>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

struct NX_VMS_API StoragePurgeStatusData
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(State,
        idle,
        inProgress,
        pending
    )

    struct LastPurgeData
    {
        std::chrono::system_clock::time_point started{std::chrono::milliseconds{-1}};
        std::chrono::milliseconds elapsed{-1};

        bool isNull() const { return started.time_since_epoch() == std::chrono::milliseconds(-1); }
    };

    QnUuid id;

    State state = State::idle;
    /**%apidoc:any */
    LastPurgeData lastPurgeData;
    /**%apidoc:any */
    double progress = 0.0;
};

using StoragePurgeStatusDataList = std::vector<StoragePurgeStatusData>;

#define LastPurgeData_Fields \
    (started) \
    (elapsed)

#define PurgeError_Fields \
    (code) \
    (failedStorages) \
    (description)

#define StoragePurgeStatusData_Fields \
    (id) \
    (state) \
    (lastPurgeData) \
    (progress)

QN_FUSION_DECLARE_FUNCTIONS(StoragePurgeStatusData::LastPurgeData, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(StoragePurgeStatusData, (json), NX_VMS_API)

} // namespace nx::vms::api
