#ifndef __API_TIME_DATA_H_
#define __API_TIME_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "utils/common/latin1_array.h"
#include "utils/common/uuid.h"


namespace ec2
{

    struct ApiTimeData: ApiData 
    {
        ApiTimeData():
            value(0),
            isPrimaryTimeServer(false)
        {}
        ApiTimeData(qint64 value):
            value(value),
            isPrimaryTimeServer(false)
        {}

        /** MSec value. */ 
        qint64 value;
        bool isPrimaryTimeServer;
        QnUuid primaryTimeServerGuid;
    };

#define ApiTimeData_Fields (value)(isPrimaryTimeServer)(primaryTimeServerGuid)

}

#endif // __API_TIME_DATA_H_
