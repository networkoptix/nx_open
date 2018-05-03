#include "tunnel_tcp_endpoint_verificator_factory.h"

#include "tunnel_tcp_available_endpoint_verificator.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

EndpointVerificatorFactory::EndpointVerificatorFactory():
    base_type(std::bind(&EndpointVerificatorFactory::defaultFactoryFunc, this,
        std::placeholders::_1))
{
}

EndpointVerificatorFactory& EndpointVerificatorFactory::instance()
{
    static EndpointVerificatorFactory instance;
    return instance;
}

std::unique_ptr<AbstractEndpointVerificator>
    EndpointVerificatorFactory::defaultFactoryFunc(const nx::String& connectSessionId)
{
    return std::make_unique<AvailableEndpointVerificator>(connectSessionId);
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
