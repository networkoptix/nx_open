#ifndef __EC2_LOCK_DATA_H_
#define __EC2_LOCK_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiLockData: ApiData
    {
        ApiLockData(): timestamp(0) {}

        QByteArray name;
        QnId peer;
        qint64 timestamp;
        QByteArray userData;
    };
#define ApiLockData_Fields (name)(peer)(timestamp)(userData)

//QN_DEFINE_STRUCT_SERIALIZATORS (ApiLockData, (name) (peer) (timestamp) (userData) )
//QN_FUSION_DECLARE_FUNCTIONS(ApiLockData, (binary))

}

#endif // __EC2_LOCK_DATA_H_
