#pragma once

#include <map>
#include <memory>

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

#include "relay_api_client.h"

namespace nx::cloud::relay::api {

using ClientFactoryFunction =
    std::unique_ptr<Client>(const nx::utils::Url& baseUrl);

/**
 * Always instantiates clien type with the highest priority.
 */
class NX_NETWORK_API ClientFactory:
    public nx::utils::BasicFactory<ClientFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<ClientFactoryFunction>;

public:
    ClientFactory();

    template<typename ClientType>
    void registerClientType(int priority = 0)
    {
        m_clientTypesByPriority.emplace(
            priority,
            [](const nx::utils::Url& baseUrl)
            {
                return std::make_unique<ClientType>(baseUrl);
            });
    }

    static ClientFactory& instance();

private:
    std::multimap<int, ClientFactory::Function, std::greater<int>>
        m_clientTypesByPriority;

    std::unique_ptr<Client> defaultFactoryFunction(
        const nx::utils::Url& baseUrl);
};

} // namespace nx::cloud::relay::api
