// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "get_version.h"

#include <nx/build_info.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/json.h>

namespace nx::network::maintenance {

Version getVersion()
{
    return Version {
        nx::build_info::vmsVersion().toStdString(),
        nx::build_info::revision().toStdString()
    };
}

void GetVersion::processRequest(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    http::RequestResult result(http::StatusCode::ok);
    result.body = std::make_unique<http::BufferSource>(
        http::header::ContentType::kJson.toString(),
        nx::reflect::json::serialize(getVersion()));

    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance
