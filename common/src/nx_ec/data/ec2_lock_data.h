#ifndef __EC2_LOCK_DATA_H_
#define __EC2_LOCK_DATA_H_

#include "api_data.h"

namespace ec2
{


struct ApiLockData: public ApiData
{
    QByteArray name;
    QnId peer;
    qint64 timestamp;
};
QN_DEFINE_STRUCT_SERIALIZATORS (ApiLockData, (name) (peer) (timestamp))

}

#endif // __EC2_LOCK_DATA_H_
