#pragma once

#include <memory>

#include "tunnel_tcp_abstract_endpoint_verificator.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

typedef std::unique_ptr<AbstractEndpointVerificator>(EndpointVerificatorFactoryFunction)(
    const nx::String& /*connectSessionId*/);

class NX_NETWORK_API EndpointVerificatorFactory:
    public nx::utils::BasicFactory<EndpointVerificatorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<EndpointVerificatorFactoryFunction>;

public:
    EndpointVerificatorFactory();

    static EndpointVerificatorFactory& instance();

private:
    std::unique_ptr<AbstractEndpointVerificator> defaultFactoryFunc(
        const nx::String& /*connectSessionId*/);
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
