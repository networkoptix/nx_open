#ifndef API_ROUTING_DATA_H
#define API_ROUTING_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiConnectionData : ApiData {
        QUuid discovererId;
        QUuid peerId;
        QString host;
        int port;
    };

#define ApiConnectionData_Fields (discovererId)(peerId)(host)(port)

} // namespace ec2

#endif // API_ROUTING_DATA_H
