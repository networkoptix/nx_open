// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "get_malloc_info.h"

#include <nx/network/http/buffer_source.h>
#include <nx/utils/memory/malloc_info.h>

namespace nx::network::maintenance {

void GetMallocInfo::processRequest(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    std::string data;
    std::string mimeType;
    if (!nx::utils::memory::mallocInfo(&data, &mimeType))
    {
        NX_DEBUG(this, nx::format("Failed to read malloc statistics. %1")
            .args(SystemError::getLastOSErrorText()));
        return completionHandler(http::StatusCode::internalServerError);
    }

    http::RequestResult result(http::StatusCode::ok);
    result.dataSource = std::make_unique<http::BufferSource>(mimeType, data);

    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance
