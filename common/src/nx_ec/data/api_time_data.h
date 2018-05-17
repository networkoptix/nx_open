#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "nx/utils/latin1_array.h"
#include "nx/fusion/model_functions_fwd.h"
#include <nx/utils/uuid.h>

namespace ec2 {

struct ApiTimeData: nx::vms::api::Data
{
    ApiTimeData() = default;

    ApiTimeData(qint64 value): value(value) {}

    /** MSec value. */
    qint64 value = 0;
    bool isPrimaryTimeServer = false;
    QnUuid primaryTimeServerGuid;
    Qn::TimeFlags syncTimeFlags = Qn::TF_none;
};
#define ApiTimeData_Fields (value)(isPrimaryTimeServer)(primaryTimeServerGuid)(syncTimeFlags)

} // namespace ec2
