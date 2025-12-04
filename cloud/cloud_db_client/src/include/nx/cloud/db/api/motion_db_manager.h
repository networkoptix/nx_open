// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>

#include "motion_db_data.h"
#include "result_code.h"
#include "time_period_data.h"

namespace nx::cloud::db::api {

/**
 * Contains actions available on analyticsDb.
 */
class MotionDbManager
{
public:

    virtual ~MotionDbManager() = default;

    /**
     * Update organization data.
     */
    virtual void saveMotion(
        const std::vector<Motion>& data,
        nx::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void getMotionPeriods(
        const api::GetMotionParamsData& filter,
        nx::MoveOnlyFunc<void(api::ResultCode, const api::TimePeriodList& data)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
