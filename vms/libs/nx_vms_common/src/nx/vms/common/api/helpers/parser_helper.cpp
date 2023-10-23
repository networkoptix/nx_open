// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "parser_helper.h"

#include <nx/utils/log/log.h>

namespace nx::vms::common::api {

nx::network::rest::Result parseRestResult(
    nx::network::http::StatusCode::Value httpStatusCode,
    Qn::SerializationFormat format,
    nx::ConstBufferRefType messageBody)
{
    // Absence of HTTP status is network error.
    if (httpStatusCode == nx::network::http::StatusCode::undefined)
        return nx::network::rest::Result::serviceUnavailable();

    // Success HTTP responses may have no body and content type.
    if (nx::network::http::StatusCode::isSuccessCode(httpStatusCode))
        return nx::network::rest::Result();

    // Support JSON format only for new REST API.
    if (format == Qn::SerializationFormat::json)
    {
        nx::network::rest::Result result;
        if (QJson::deserialize(messageBody, &result))
            return result;
    }

    constexpr auto kMessageBodyLogSize = 50;
    NX_DEBUG(NX_SCOPE_TAG,
        "Unsupported format '%1', status code: %2, message body: %3 ...",
        nx::reflect::enumeration::toString(format),
        httpStatusCode,
        messageBody.substr(0, kMessageBodyLogSize));

    return nx::network::rest::Result(
        nx::network::rest::Result::errorFromHttpStatus(httpStatusCode));
}

} // namespace nx::vms::common::api
