// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/network/stun/message.h>
#include <nx/reflect/enum_instrument.h>

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

} // nx::hpm::api
