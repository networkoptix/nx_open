#ifndef API_SYSTEM_NAME_DATA_H
#define API_SYSTEM_NAME_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiSystemNameData : ApiData {
        QString systemName;
        QString targetId;
    };

#define ApiSystemNameData_Fields (systemName)(targetId)

} // namespace ec2

#endif // API_SYSTEM_NAME_DATA_H
