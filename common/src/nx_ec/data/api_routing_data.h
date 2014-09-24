#ifndef API_ROUTING_DATA_H
#define API_ROUTING_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiConnectionData : ApiData {
        QUuid peerId;
        QString host;
        int port;

        bool operator ==(const ApiConnectionData &other) const {
            return  peerId == other.peerId &&
                    host == other.host &&
                    port == other.port;
        }
    };

#define ApiConnectionData_Fields (peerId)(host)(port)

} // namespace ec2

#endif // API_ROUTING_DATA_H
