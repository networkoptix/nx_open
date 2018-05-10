#pragma once

#include "api_data.h"

#include <nx_ec/transaction_timestamp.h>

namespace ec2 {

struct ApiSystemIdData: ApiData
{
    ApiSystemIdData(): sysIdTime(0) {}
    ApiSystemIdData(const QString &systemId): systemId(systemId), sysIdTime(0) {}

    QnUuid systemId;
    qint64 sysIdTime;
    Timestamp tranLogTime;
};
#define ApiSystemIdData_Fields (systemId)(sysIdTime)(tranLogTime)

} // namespace ec2
