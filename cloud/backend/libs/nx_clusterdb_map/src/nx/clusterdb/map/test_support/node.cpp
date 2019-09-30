#include "node.h"

#include <nx/sql/async_sql_query_executor.h>
#include <nx/clusterdb/engine/service/view.h>
#include <nx/clusterdb/engine/service/model.h>

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
        nodeSettings.synchronization().clusterId);
}

void Node::teardown()
{
    m_database.reset();
}

//-------------------------------------------------------------------------------------------------
// EmbeddedNode

EmbeddedNode::EmbeddedNode(int argc, char** argv):
    base_type(argc, argv, QString("EmbeddedNode_") + kApplicationId)
{
}

std::unique_ptr<utils::AbstractServiceSettings> EmbeddedNode::createSettings()
{
    return std::make_unique<EmbeddedNodeSettings>();
}

int EmbeddedNode::serviceMain(const utils::AbstractServiceSettings& settings)
{
    const EmbeddedNodeSettings& nodeSettings = static_cast<const EmbeddedNodeSettings&>(settings);

    m_sqlQueryExecutor = std::make_unique<nx::sql::AsyncSqlQueryExecutor>(nodeSettings.sql);

    m_database = std::make_unique<map::EmbeddedDatabase>(
        nodeSettings.map,
        m_sqlQueryExecutor.get());

    View view(nodeSettings, nodeSettings.api().baseHttpPath, &m_database->synchronizationEngine());
    m_view = &view;

    return runMainLoop();
}

map::EmbeddedDatabase& EmbeddedNode::database()
{
    return *m_database;
}

std::vector<nx::network::SocketAddress> EmbeddedNode::httpEndpoints()
{
    return m_view->httpEndpoints();
}

//-------------------------------------------------------------------------------------------------
// EmbeddedNode::View

EmbeddedNode::View::View(
    const nx::clusterdb::engine::Settings& settings,
    const std::string& baseRequestPath,
    nx::clusterdb::engine::SynchronizationEngine* syncEngine)
    :
    httpServer(nx::network::http::server::Builder::build(
        settings.http(),
        nullptr,
        &httpMessageDispatcher))
{
    syncEngine->registerHttpApi(
        baseRequestPath,
        &httpMessageDispatcher);
}

std::vector<nx::network::SocketAddress> EmbeddedNode::View::httpEndpoints()
{
    return httpServer->endpoints();
}

void EmbeddedNode::View::listen()
{
    httpServer->listen();
}


} // namespace nx::clusterdb::map::test