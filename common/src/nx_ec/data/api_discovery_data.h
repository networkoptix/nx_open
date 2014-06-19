#ifndef API_DISCOVERY_DATA_H
#define API_DISCOVERY_DATA_H

#include "api_data.h"

namespace ec2 {

    struct ApiDiscoveryData : ApiIdData {
        QString url;
        bool ignore;
    };

#define ApiDiscoveryData_Fields ApiIdData_Fields(url)(ignore)(id)

    struct ApiDiscoverPeerData : ApiData {
        QString url;
    };
#define ApiDiscoverPeerData_Fields (url)

} // namespace ec2

#endif // API_DISCOVERY_DATA_H
