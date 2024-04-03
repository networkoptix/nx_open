// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/network/http/server/api_request_result.h>
#include <nx/network/stun/message.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/string.h>

namespace nx::hpm::api {

NX_REFLECTION_ENUM_CLASS(ResultCode,
    ok = 0,
    networkError,
    responseParseError,
    notAuthorized,
    badRequest,
    notFound,
    otherLogicError,
    notImplemented,
    noSuitableConnectionMethod,
    timedOut,
    serverConnectionBroken,
    noReplyFromServer,
    badTransport,
    interrupted,
    tryAlternate,
    internalServerError
)

NX_NETWORK_API ResultCode fromStunErrorToResultCode(
    const nx::network::stun::attrs::ErrorCode& errorCode);

NX_NETWORK_API int resultCodeToStunErrorCode(ResultCode resultCode);

NX_NETWORK_API void PrintTo(ResultCode val, ::std::ostream* os);

//-------------------------------------------------------------------------------------------------

struct NX_NETWORK_API Result
{
    ResultCode code = ResultCode::ok;
    std::string text;

    Result(): Result(ResultCode::ok) {}
    Result(ResultCode code): code(code), text(nx::reflect::toString(code)) {}
    Result(ResultCode code, std::string text): code(code), text(std::move(text)) {}

    bool ok() const { return code == ResultCode::ok; }
    std::string toString() const;
};

NX_REFLECTION_INSTRUMENT(Result, (code)(text))

NX_NETWORK_API void convert(const Result& result, nx::network::http::ApiRequestResult* httpResult);
NX_NETWORK_API nx::network::http::ApiRequestResult toApiResult(const Result& result);

} // nx::hpm::api
