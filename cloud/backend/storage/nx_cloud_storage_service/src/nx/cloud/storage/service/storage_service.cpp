#include "storage_service.h"

#include "settings.h"

namespace nx::cloud::storage::service {

StorageService::StorageService(int argc, char** argv):
    base_type(argc, argv, "nx_cloud_storage_service")
{
}

std::vector<network::SocketAddress> StorageService::httpEndpoints() const
{
    // TODO
    return {};
}

std::vector<network::SocketAddress> StorageService::httpsEndpoints() const
{
    // TODO
    return {};
}

std::unique_ptr<utils::AbstractServiceSettings> StorageService::createSettings()
{
    return std::make_unique<Settings>();
}

int StorageService::serviceMain(const utils::AbstractServiceSettings& settings)
{
    // TODO
    return runMainLoop();
}

} // namespace nx::cloud::storage::service
