// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "get_version.h"

#include <nx/build_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_types.h>

namespace nx::network::maintenance {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Version, (json), Version_Fields)

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
    result.dataSource = std::make_unique<http::BufferSource>(
        http::header::ContentType::kJson.toString(),
        QJson::serialized(getVersion()));

    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance
