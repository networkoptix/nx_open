// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/network/stun/message.h>
#include <nx/reflect/enum_instrument.h>
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
    tryAlternate
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

    bool ok() const { return code == ResultCode::ok; }
    std::string toString() const;
};

NX_REFLECTION_INSTRUMENT(Result, (code)(text))

} // nx::hpm::api
