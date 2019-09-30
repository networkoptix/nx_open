#include "storage_service.h"

#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/clusterdb/engine/http/http_paths.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "settings.h"
#include "controller/controller.h"
#include "model/model.h"
#include "model/database.h"
#include "view/view.h"

namespace nx::cloud::storage::service {

StorageService::StorageService(int argc, char** argv):
    base_type(argc, argv, "nx_cloud_storage_service")
{
}

const conf::Settings& StorageService::settings() const
{
    return *m_settings;
}

std::vector<network::SocketAddress> StorageService::httpEndpoints() const
{
    return m_view->httpServer()->httpEndpoints();
}

std::vector<network::SocketAddress> StorageService::httpsEndpoints() const
{
    return m_view->httpServer()->httpsEndpoints();
}

std::unique_ptr<utils::AbstractServiceSettings> StorageService::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int StorageService::serviceMain(const utils::AbstractServiceSettings& settings)
{
    const conf::Settings& storageSettings = static_cast<const conf::Settings&>(settings);
    m_settings = &storageSettings;

    model::Model model(storageSettings);
    m_model = &model;

    controller::Controller controller(storageSettings, m_model);
    m_controller = &controller;

    view::View view(storageSettings, m_controller);
    m_view = &view;

    view.listen();

    NX_INFO(this, "Starting Cloud Storage Service...");

    registerThisInstanceInCluster();

    int result = runMainLoop();

    view.stop();
    controller.stop();
    model.stop();

    NX_INFO(this, "Cloud Storage Service stopped");

    return result;
}

void StorageService::registerThisInstanceInCluster()
{
    using namespace nx::network;

    if (m_settings->server().name.empty())
    {
        NX_INFO(this, "Server name is empty, discovery will not start");
        return;
    }

    m_model->database().syncEngine().registerHttpApi(
        clusterdb::engine::kBaseSynchronizationPath,
        &m_view->httpServer()->messageDispatcher());

    const nx::utils::Url syncEngineUrl = url::Builder()
        .setScheme(http::kUrlSchemeName)
        .setHost(m_settings->server().name.c_str())
        .setPath(http::rest::substituteParameters(
            clusterdb::engine::kBaseSynchronizationPath,
            {m_settings->database().synchronization.clusterId}));

    m_model->database().syncEngine().discoveryManager().start(
        m_settings->database().synchronization.clusterId,
        syncEngineUrl);
}

} // namespace nx::cloud::storage::service
