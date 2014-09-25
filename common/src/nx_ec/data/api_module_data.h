#ifndef API_MODULE_DATA_H
#define API_MODULE_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiModuleData : ApiData {
        QString type;       // e.g. "Media Server"
        QnUuid id;
        QString systemName;
        QString version;
        QString systemInformation;
        QStringList addresses;
        int port;
        bool isAlive;
        QList<QnUuid> discoverers;
    };

#define ApiModuleData_Fields (type)(id)(systemName)(version)(systemInformation)(addresses)(port)(isAlive)(discoverers)

} // namespace ec2


#endif // API_MODULE_DATA_H
