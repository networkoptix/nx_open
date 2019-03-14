#include "node.h"

namespace nx::clusterdb::map::test {

namespace {

static constexpr char kApplicationId[] = "nx::clusterdb::map::node";

}

Node::Node(int argc, char** argv):
    nx::clusterdb::engine::Service(kApplicationId, argc, argv)
{
}

map::Database& Node::database()
{
    return *m_database.get();
}

void Node::setup()
{
    m_database = std::make_unique<Database>(&syncronizationEngine(), &sqlQueryExecutor());
}

void Node::teardown()
{
    m_database.reset();
}

} // namespace nx::clusterdb::map::test