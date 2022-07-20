// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_handler_redirect.h"

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "../../buffer_source.h"

namespace nx::network::http::server::handler {

Redirect::Redirect(const nx::utils::Url& actualLocation):
    m_actualLocation(actualLocation)
{
}

void Redirect::processRequest(
    RequestContext /*requestContext*/,
    RequestProcessedHandler completionHandler)
{
    RequestResult result(StatusCode::movedPermanently);
    result.headers.emplace("Location", m_actualLocation.toStdString());
    completionHandler(std::move(result));
}

} // namespace nx::network::http::server::handler
