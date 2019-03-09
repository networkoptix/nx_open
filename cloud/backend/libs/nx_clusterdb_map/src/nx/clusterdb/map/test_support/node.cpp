#include "node.h"

#include "node_settings.h"

namespace nx::clusterdb::map::test {

namespace {

static constexpr char kApplicationId[] = "nx::clusterdb::map::node";

} // namespace

Node::Node(int argc, char** argv):
    nx::clusterdb::engine::Service(kApplicationId, argc, argv)
{
}

map::Database& Node::database()
{
    return *m_database.get();
}

std::unique_ptr<utils::AbstractServiceSettings> Node::createSettings()
{
    return std::make_unique<NodeSettings>();
}

void Node::setup(const utils::AbstractServiceSettings& settings)
{
    const NodeSettings& nodeSettings = static_cast<const NodeSettings&>(settings);

    m_database = std::make_unique<Database>(
        &synchronizationEngine(),
        &sqlQueryExecutor(),
        nodeSettings.map.clusterId);
}

void Node::teardown()
{
    m_database.reset();
}

} // namespace nx::clusterdb::map::test