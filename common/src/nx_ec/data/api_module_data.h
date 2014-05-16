#ifndef API_MODULE_DATA_H
#define API_MODULE_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiModuleData : ApiData {
        QString type;       // e.g. "Media Server"
        QnId id;
        QString systemName;
        QString version;
        QString systemInformation;
        int port;
        bool isAlive;
    };

#define ApiModuleData_Fields (type)(id)(systemName)(version)(systemInformation)(port)(isAlive)

} // namespace ec2


#endif // API_MODULE_DATA_H
