#ifndef API_MODULE_DATA_H
#define API_MODULE_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiModuleData : ApiData {
        QString type;       // e.g. "Media Server"
        QString customization;
        QnUuid id;
        QString systemName;
        QString version;
        QString systemInformation;
        QStringList addresses;
        int port;
        QString name;
        bool sslAllowed;
        QByteArray authHash;

        bool isAlive;
    };

#define ApiModuleData_Fields (type)(id)(systemName)(version)(systemInformation)(addresses)(port)(isAlive)(customization)(name)(sslAllowed)(authHash)

} // namespace ec2


#endif // API_MODULE_DATA_H
