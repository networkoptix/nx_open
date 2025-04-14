// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "get_malloc_info.h"

#include <nx/network/http/buffer_source.h>
#include <nx/reflect/json.h>
#include <nx/utils/memory/malloc_info.h>

namespace nx::network::maintenance {

void GetMallocInfo::processRequest(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    std::string data;
    std::string mimeType;
    if (!nx::utils::memory::mallocInfo(&data, &mimeType))
    {
        NX_DEBUG(this, "Failed to read malloc statistics. %1", SystemError::getLastOSErrorText());
        return completionHandler(http::StatusCode::internalServerError);
    }

    http::RequestResult result(http::StatusCode::ok);

    auto& url = requestContext.request.requestLine.url;
    if (url.hasQueryItem("xml") && (url.queryItem("xml").isEmpty() || url.queryItem("xml") == "1"))
    {
        result.body = std::make_unique<http::BufferSource>(mimeType, data);
    }
    else
    {
        const auto mallocInfo = nx::utils::memory::fromXml(data);
        result.body = std::make_unique<http::BufferSource>(
            "application/json",
            nx::reflect::json::serialize(mallocInfo));
    }

    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance
