#include "traffic_relay.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

void TrafficRelay::startRelaying(
    RelayConnectionData /*clientConnection*/,
    RelayConnectionData /*serverConnection*/)
{
    // TODO
}

void TrafficRelay::terminateAllConnectionsByPeerId(
    const std::string& /*peerId*/)
{
    // TODO
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
