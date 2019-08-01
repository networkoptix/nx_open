#include "node_launcher.h"

#include <nx/network/url/url_builder.h>

namespace nx::clusterdb::map::test {

static constexpr char kClusterId[] = "clusterDbMapTest";

NodeLauncher::NodeLauncher()
{
    addArg("--http/listenOn=127.0.0.1:0");
    addArg("-p2pDb/clusterId", kClusterId);
}

NodeLauncher::~NodeLauncher()
{
    stop();
}

} // namespace nx::clusterdb::map::test