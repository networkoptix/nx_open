#ifndef __EC2_LOCK_DATA_H_
#define __EC2_LOCK_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiLockData: ApiData
    {
        ApiLockData(): timestamp(0) {}

        QString name;
        QnUuid peer;
        qint64 timestamp;
        QByteArray userData;
    };
#define ApiLockData_Fields (name)(peer)(timestamp)(userData)

} // namespace ec2

#endif // __EC2_LOCK_DATA_H_
