// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include "tunnel_tcp_abstract_endpoint_verificator.h"

namespace nx::network::cloud::tcp {

using EndpointVerificatorFactoryFunction =
    std::unique_ptr<AbstractEndpointVerificator>(const std::string& /*connectSessionId*/);

class NX_NETWORK_API EndpointVerificatorFactory:
    public nx::utils::BasicFactory<EndpointVerificatorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<EndpointVerificatorFactoryFunction>;

public:
    EndpointVerificatorFactory();

    static EndpointVerificatorFactory& instance();

private:
    std::unique_ptr<AbstractEndpointVerificator> defaultFactoryFunc(
        const std::string& /*connectSessionId*/);
};

} // namespace nx::network::cloud::tcp
