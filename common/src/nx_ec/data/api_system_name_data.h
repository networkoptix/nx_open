#pragma once

#include "api_data.h"

#include <nx_ec/transaction_timestamp.h>

namespace ec2 {

struct ApiSystemIdData: nx::vms::api::Data
{
    ApiSystemIdData() = default;
    ApiSystemIdData(const QString &systemId): systemId(systemId) {}

    QnUuid systemId;
    qint64 sysIdTime = 0;
    Timestamp tranLogTime;
};
#define ApiSystemIdData_Fields (systemId)(sysIdTime)(tranLogTime)

} // namespace ec2
