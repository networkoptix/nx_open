#include "service_factory.h"

#include <nx/vms/server/nvr/i_service.h>

namespace nx::vms::server::nvr {

void ServiceFactory::registerServiceProvider(std::unique_ptr<IServiceProvider> serviceProvider)
{
    m_serviceProviders.push_back(std::move(serviceProvider));
}

std::unique_ptr<IService> ServiceFactory::createService() const
{
    for (const auto& serviceProvider: m_serviceProviders)
    {
        if (std::unique_ptr<IService> service = serviceProvider->createService())
            return service;
    }

    return nullptr;
}

} // namespace nx::vms::server::nvr
