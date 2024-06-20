// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "result_code.h"

#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/reflect/string_conversion.h>

namespace nx::hpm::api {

ResultCode fromStunErrorToResultCode(
    const nx::network::stun::attrs::ErrorCode& errorCode)
{
    switch (errorCode.getCode())
    {
        case nx::network::stun::error::badRequest:
            return ResultCode::badRequest;
        case nx::network::stun::error::unauthorized:
            return ResultCode::notAuthorized;
        case nx::network::stun::extension::error::notFound:
            return ResultCode::notFound;
        case nx::network::stun::error::tryAlternate:
            return ResultCode::tryAlternate;
        default:
            return ResultCode::otherLogicError;
    }
}

int resultCodeToStunErrorCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::badRequest:
            return nx::network::stun::error::badRequest;
        case ResultCode::notAuthorized:
            return nx::network::stun::error::unauthorized;
        case ResultCode::notFound:
            return nx::network::stun::extension::error::notFound;
        case ResultCode::tryAlternate:
            return nx::network::stun::error::tryAlternate;
        default:
            return nx::network::stun::error::serverError;
    }
}

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << nx::reflect::toString(val);
}

//-------------------------------------------------------------------------------------------------

std::string Result::toString() const
{
    return nx::utils::buildString("code: ", nx::reflect::toString(code), ", text: ", text);
}

using namespace nx::network::http;

static constexpr std::pair<api::ResultCode, ApiRequestErrorClass>
    kResultCodeConversionTable[] =
{
    {ResultCode::notAuthorized, ApiRequestErrorClass::unauthorized},
    {ResultCode::badRequest, ApiRequestErrorClass::badRequest},
    {ResultCode::notFound, ApiRequestErrorClass::notFound},
    {ResultCode::internalServerError, ApiRequestErrorClass::internalError}
};

void convert(const Result& result, ApiRequestResult* httpResult)
{
    *httpResult = ApiRequestResult();

    if (!result.ok())
    {
        httpResult->setErrorClass(ApiRequestErrorClass::logicError);

        for (const auto& [from, to]: kResultCodeConversionTable)
        {
            if (result.code == from)
                httpResult->setErrorClass(to);
        }

        httpResult->setResultCode(nx::reflect::toString(result.code));
        httpResult->setErrorText(result.text);
        httpResult->setErrorDetail(static_cast<int>(result.code));
    }
}

nx::network::http::ApiRequestResult toApiResult(const Result& result)
{
    ApiRequestResult apiResult;
    convert(result, &apiResult);
    return apiResult;
}

} // namespace nx::hpm::api
