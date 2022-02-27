// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tunnel_tcp_endpoint_verificator_factory.h"

#include "tunnel_tcp_available_endpoint_verificator.h"

namespace nx::network::cloud::tcp {

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
    EndpointVerificatorFactory::defaultFactoryFunc(const std::string& connectSessionId)
{
    return std::make_unique<AvailableEndpointVerificator>(connectSessionId);
}

} // namespace nx::network::cloud::tcp
