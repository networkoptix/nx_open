#ifndef __API_TIME_DATA_H_
#define __API_TIME_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "utils/common/latin1_array.h"

namespace ec2
{

    struct ApiTimeData: ApiData 
    {
        ApiTimeData(): value(0) {}
        ApiTimeData(qint64 value): value(value) {}

        /** MSec value. */ 
        qint64 value;
    };

#define ApiTimeData_Fields (value)

}

#endif // __API_TIME_DATA_H_
