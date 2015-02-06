#ifndef API_SYSTEM_NAME_DATA_H
#define API_SYSTEM_NAME_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiSystemNameData : ApiData {
        ApiSystemNameData(): sysIdTime(0) {}
        ApiSystemNameData(const QString &systemName): systemName(systemName) {}

        QString systemName;
        qint64 sysIdTime;
    };
    #define ApiSystemNameData_Fields (systemName)(sysIdTime)

} // namespace ec2

#endif // API_SYSTEM_NAME_DATA_H
