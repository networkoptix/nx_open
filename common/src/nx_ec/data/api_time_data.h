#ifndef __API_TIME_DATA_H_
#define __API_TIME_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "utils/common/latin1_array.h"
#include "utils/common/model_functions_fwd.h"
#include <nx/utils/uuid.h>


namespace ec2
{
    struct ApiTimeData: ApiData 
    {
        ApiTimeData():
            value(0),
            isPrimaryTimeServer(false),
            syncTimeFlags(Qn::TF_none)
        {}
        ApiTimeData(qint64 value):
            value(value),
            isPrimaryTimeServer(false),
            syncTimeFlags(Qn::TF_none)
        {}

        /** MSec value. */ 
        qint64 value;
        bool isPrimaryTimeServer;
        QnUuid primaryTimeServerGuid;
        Qn::TimeFlags syncTimeFlags;
    };

#define ApiTimeData_Fields (value)(isPrimaryTimeServer)(primaryTimeServerGuid)(syncTimeFlags)

}

#endif // __API_TIME_DATA_H_
