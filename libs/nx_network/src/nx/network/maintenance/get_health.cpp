// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "get_health.h"

namespace nx::network::maintenance {

void GetHealth::processRequest(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    // TODO: Report application-specific health information.

    completionHandler(http::StatusCode::ok);
}

} // namespace nx::network::maintenance
