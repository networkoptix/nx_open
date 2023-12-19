// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <functional>

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/generic_api_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/db/api/result_code.h>

#include "data/types.h"

namespace nx::cloud::db::client {

struct ResultCodeDescriptor
{
    using ResultCode = api::ResultCode;

    template<typename... Output>
    static auto getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const network::http::Response* response,
        const network::http::ApiRequestResult& /*fusionRequestResult*/,
        const Output&... /*output*/)
    {
        if (systemErrorCode != SystemError::noError)
            return api::ResultCode::networkError;

        const auto resultCodeStrIter =
            response->headers.find(Qn::API_RESULT_CODE_HEADER_NAME);
        if (resultCodeStrIter != response->headers.end())
        {
            return nx::reflect::fromString(
                resultCodeStrIter->second, api::ResultCode::unknownError);
        }
        else
        {
            return api::httpStatusCodeToResultCode(
                static_cast<nx::network::http::StatusCode::Value>(
                    response->statusLine.statusCode));
        }
    }
};

class ApiRequestsExecutor:
    public nx::network::http::GenericApiClient<ResultCodeDescriptor>
{
    using base_type = nx::network::http::GenericApiClient<ResultCodeDescriptor>;

public:
    using base_type::base_type;
    using base_type::makeAsyncCall;
};

} // namespace nx::cloud::db::client
