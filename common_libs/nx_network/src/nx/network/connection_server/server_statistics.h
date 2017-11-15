#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace network {
namespace server {

struct Statistics
{
    int connectionCount = 0;
    int connectionsAcceptedPerMinute = 0;
    int requestsServedPerMinute = 0;
    /**
     * Calculated for connections closed in the last minute.
     */
    int requestsAveragePerConnection = 0;
};

#define Statistics_server_Fields (connectionCount)(connectionsAcceptedPerMinute) \
    (requestsServedPerMinute)(requestsAveragePerConnection)

bool NX_NETWORK_API deserialize(QnJsonContext*, const QJsonValue&, Statistics*);
void NX_NETWORK_API serialize(QnJsonContext*, const Statistics&, class QJsonValue*);

} // namespace server
} // namespace network
} // namespace nx
