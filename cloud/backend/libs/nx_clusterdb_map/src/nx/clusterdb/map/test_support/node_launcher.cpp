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

EmbeddedNodeLauncher::EmbeddedNodeLauncher()
{
    addArg("--http/listenOn=127.0.0.1:0");
    addArg("-p2pDb/clusterId", kClusterId);
    addArg("-db/driverName", "QSQLITE");
    std::string dbName = lm("%1/embedded_node_db.sqlite").arg(testDataDir()).toStdString();
    addArg("-db/name", dbName.c_str());
}

EmbeddedNodeLauncher::~EmbeddedNodeLauncher()
{
    stop();
}

} // namespace nx::clusterdb::map::test