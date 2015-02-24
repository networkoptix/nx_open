#ifndef API_SYSTEM_NAME_DATA_H
#define API_SYSTEM_NAME_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiSystemNameData : ApiData {
        ApiSystemNameData(): sysIdTime(0), tranLogTime(0) {}
        ApiSystemNameData(const QString &systemName): systemName(systemName), sysIdTime(0), tranLogTime(0) {}

        QString systemName;
        qint64 sysIdTime;
        qint64 tranLogTime;
    };
    #define ApiSystemNameData_Fields (systemName)(sysIdTime)(tranLogTime)

} // namespace ec2

#endif // API_SYSTEM_NAME_DATA_H
