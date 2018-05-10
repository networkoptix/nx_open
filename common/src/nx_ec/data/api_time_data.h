#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "nx/utils/latin1_array.h"
#include "nx/fusion/model_functions_fwd.h"
#include <nx/utils/uuid.h>

namespace ec2 {

struct ApiTimeData: ApiData
{
    ApiTimeData():
        value(0),
        isPrimaryTimeServer(false),
        syncTimeFlags(Qn::TF_none)
    {
    }

    ApiTimeData(qint64 value):
        value(value),
        isPrimaryTimeServer(false),
        syncTimeFlags(Qn::TF_none)
    {
    }

    /** MSec value. */
    qint64 value;
    bool isPrimaryTimeServer;
    QnUuid primaryTimeServerGuid;
    Qn::TimeFlags syncTimeFlags;
};
#define ApiTimeData_Fields (value)(isPrimaryTimeServer)(primaryTimeServerGuid)(syncTimeFlags)

} // namespace ec2
