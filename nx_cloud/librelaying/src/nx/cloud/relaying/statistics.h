#pragma once

namespace nx {
namespace cloud {
namespace relaying {

struct Statistics
{
    int listeningServerCount = 0;
    int connectionsCount = 0;
    int connectionsAveragePerServerCount = 0;
    int connectionsAcceptedPerMinute = 0;
};

} // namespace relaying
} // namespace cloud
} // namespace nx
