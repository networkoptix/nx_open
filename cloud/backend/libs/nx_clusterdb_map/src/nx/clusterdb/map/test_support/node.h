#pragma once

#include <nx/clusterdb/engine/service/service.h>

#include "../embedded_database.h"
#include "../cache.h"

namespace nx::network::http::server { class HttpServer; }

namespace nx::clusterdb::map::test {

class NX_KEY_VALUE_DB_API Node:
    public nx::clusterdb::engine::Service
{
public:
    Node(int argc, char** argv);

    map::Database& database();

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;

    virtual void setup(const nx::utils::AbstractServiceSettings& settings) override;
    virtual void teardown() override;

private:
    std::unique_ptr<map::Database> m_database;
};

//-------------------------------------------------------------------------------------------------
// EmbeddedNode

class NX_KEY_VALUE_DB_API EmbeddedNode:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;
public:
    EmbeddedNode(int argc, char** argv);

    map::EmbeddedDatabase& database();

    std::vector<nx::network::SocketAddress> httpEndpoints();

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;

private:
    struct View
    {
        nx::network::http::server::rest::MessageDispatcher httpMessageDispatcher;
        std::unique_ptr<nx::network::http::server::HttpServer> httpServer;

        View(const nx::clusterdb::engine::Settings& settings,
            const std::string& baseRequestPath,
            nx::clusterdb::engine::SynchronizationEngine* syncEngine);

        std::vector<nx::network::SocketAddress> httpEndpoints();
        void listen();
    };

    View* m_view = nullptr;
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_sqlQueryExecutor;
    std::unique_ptr<map::EmbeddedDatabase> m_database;

};

} // namespace nx::clusterdb::map::test
