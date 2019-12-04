#pragma once

#include <vector>

#include <nx/vms/server/nvr/i_service_provider.h>

namespace nx::vms::server::nvr {

class ServiceFactory
{
public:
    ServiceFactory() = default;

    void registerServiceProvider(std::unique_ptr<IServiceProvider> serviceProvider);

    std::unique_ptr<IService> createService() const;

private:
    std::vector<std::unique_ptr<IServiceProvider>> m_serviceProviders;

};

} // namespace nx::vms::server::nvr
