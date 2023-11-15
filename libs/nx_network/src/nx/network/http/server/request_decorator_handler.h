// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_http_request_handler.h"

namespace nx::network::http::server {

/**
 * Modifies request before passing it to the next handler.
 */
class NX_NETWORK_API RequestDecoratorHandler:
    public IntermediaryHandler
{
    using base_type = IntermediaryHandler;

public:
    using base_type::base_type;

    virtual void serve(
        RequestContext requestContext,
        nx::utils::MoveOnlyFunc<void(RequestResult)> handler) override;

    /**
     * Attributes specified here are added to each request.
     * Already-existing attributes are overwritten.
     */
    void addAttributes(const std::vector<std::pair<std::string, std::string>>& attributes);

private:
    std::vector<std::pair<std::string, std::string>> m_attributes;
};

} // namespace nx::network::http::server
