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
        int protoVersion;

        bool isAlive;

        ApiModuleData() : port(0), sslAllowed(false), protoVersion(0), isAlive(false) {}
    };

#define ApiModuleData_Fields (type)(id)(systemName)(version)(systemInformation)(addresses)(port)(isAlive)(customization)(name)(sslAllowed)(authHash)(protoVersion)

} // namespace ec2


#endif // API_MODULE_DATA_H
