#ifndef __EC2_LOCK_DATA_H_
#define __EC2_LOCK_DATA_H_

#include "api_data.h"

namespace ec2
{


struct ApiLockData: public ApiData
{
    ApiLockData(): timestamp(0) {}

    QByteArray name;
    QnId peer;
    qint64 timestamp;
    QByteArray userData;
    QnId originator;
};
QN_DEFINE_STRUCT_SERIALIZATORS (ApiLockData, (name) (peer) (timestamp) (userData) (originator) )

}

#endif // __EC2_LOCK_DATA_H_
