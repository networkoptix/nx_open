#ifndef API_SYSTEM_NAME_DATA_H
#define API_SYSTEM_NAME_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiSystemNameData : ApiData {
        ApiSystemNameData() {}
        ApiSystemNameData(const QString &systemName): systemName(systemName) {}

        QString systemName;
    };
    #define ApiSystemNameData_Fields (systemName)

} // namespace ec2

#endif // API_SYSTEM_NAME_DATA_H
