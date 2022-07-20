// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx::network::maintenance {

class GetMallocInfo:
    public http::RequestHandlerWithContext
{
protected:
    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler) override;
};

} // namespace nx::network::maintenance
