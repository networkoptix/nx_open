#ifndef __API_TIME_DATA_H_
#define __API_TIME_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "nx/utils/latin1_array.h"
#include "nx/fusion/model_functions_fwd.h"
#include <nx/utils/uuid.h>


namespace ec2
{
    struct ApiTimeData: nx::vms::api::Data 
    {
        ApiTimeData() = default;

        ApiTimeData(qint64 value):
            value(value)
        {}

        /** MSec value. */ 
        qint64 value = 0;
        bool isPrimaryTimeServer = 0;
        QnUuid primaryTimeServerGuid;
        Qn::TimeFlags syncTimeFlags = Qn::TF_none;
    };

#define ApiTimeData_Fields (value)(isPrimaryTimeServer)(primaryTimeServerGuid)(syncTimeFlags)

}

#endif // __API_TIME_DATA_H_
