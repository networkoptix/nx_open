#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/utils/service.h>
#include <nx/utils/url.h>

namespace nx::sql { class AsyncSqlQueryExecutor; }

namespace nx::clusterdb::engine {

class Controller;
class Model;
class View;
class Settings;
class SyncronizationEngine;

class NX_DATA_SYNC_ENGINE_API Service:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    Service(
        const std::string& applicationId,
        int argc,
        char **argv);

    std::vector<network::SocketAddress> httpEndpoints() const;

    void connectToNode(
        const std::string& systemId,
        const nx::utils::Url& url);

    SyncronizationEngine& syncronizationEngine();

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;

    /**
     * Override this method to initialize custom logic.
     * E.g., register commands to synchronize and event handles.
     */
    virtual void setup() {}
    virtual void teardown() {}

    nx::sql::AsyncSqlQueryExecutor& sqlQueryExecutor();

private:
    const std::string m_applicationId;
    const Settings* m_settings = nullptr;
    Model* m_model = nullptr;
    Controller* m_controller = nullptr;
    View* m_view = nullptr;
};

} // namespace nx::clusterdb::engine
