#include "storage_service.h"

#include "settings.h"
#include "controller/controller.h"
#include "view/view.h"

namespace nx::cloud::storage::service {

StorageService::StorageService(int argc, char** argv):
    base_type(argc, argv, "nx_cloud_storage_service")
{
}

const Settings& StorageService::settings() const
{
    return *m_settings;
}

std::vector<network::SocketAddress> StorageService::httpEndpoints() const
{
    return m_view->httpServer().httpEndpoints();
}

std::vector<network::SocketAddress> StorageService::httpsEndpoints() const
{
    return m_view->httpServer().httpsEndpoints();
}

std::unique_ptr<utils::AbstractServiceSettings> StorageService::createSettings()
{
    return std::make_unique<Settings>();
}

int StorageService::serviceMain(const utils::AbstractServiceSettings& settings)
{
    const Settings& storageSettings = static_cast<const Settings&>(settings);
    m_settings = &storageSettings;

    Controller controller(storageSettings);
    m_controller = &controller;

    View view(storageSettings, m_controller);
    m_view = &view;

    view.listen();

    NX_INFO(this, "Starting Cloud Storage Service...");

    int result = runMainLoop();

    view.stop();

    NX_INFO(this, "Cloud Storage Service stopped");

    return result;
}

} // namespace nx::cloud::storage::service
