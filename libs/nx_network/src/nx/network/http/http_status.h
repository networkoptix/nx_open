// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/reflect/instrument.h>

namespace nx::network::http::StatusCode {

/**
 * enum has name "Value" for unification purpose only. Real enumeration name is StatusCode (name of namespace).
 * Enum is referred to as StatusCode::Value, if assign real name to enum it will be StatusCode::StatusCode.
 * Namespace is used to associate enum with corresponding functions like toString() and fromString().
 * Namespace is required, since not all such functions can be put to global namespace.
 * E.g. this namespace has convenience function toString(int httpStatusCode).
 */
enum Value: int
{
    undefined = 0,
    _continue = 100,
    switchingProtocols = 101,

    ok = 200,
    created = 201,
    accepted = 202,
    noContent = 204,
    partialContent = 206,
    multiStatus = 207,
    lastSuccessCode = 299,

    multipleChoices = 300,
    movedPermanently = 301,
    movedTemporarily = 302,
    found = 302,
    seeOther = 303,
    notModified = 304,
    useProxy = 305,
    temporaryRedirect = 307,
    permanentRedirect = 308,

    badRequest = 400,
    unauthorized = 401,
    paymentRequired = 402,
    forbidden = 403,
    notFound = 404,
    notAllowed = 405,
    notAcceptable = 406,
    proxyAuthenticationRequired = 407,
    requestTimeOut = 408,
    conflict = 409,
    gone = 410,
    lengthRequired = 411,
    preconditionFailed = 412,
    requestEntityTooLarge = 413,
    requestUriToLarge = 414,
    unsupportedMediaType = 415,
    rangeNotSatisfiable = 416,
    unprocessableEntity = 422,
    tooManyRequests = 429,
    unavailableForLegalReasons = 451,

    internalServerError = 500,
    notImplemented = 501,
    badGateway = 502,
    serviceUnavailable = 503,
    gatewayTimeOut = 504,
};

NX_REFLECTION_TAG_TYPE(Value, useStringConversionForSerialization)

NX_NETWORK_API std::string toString(Value);
NX_NETWORK_API std::string toString(int);

/** Returns true if  statusCode is 2xx */
NX_NETWORK_API bool isSuccessCode(Value statusCode);
/** Returns true if  statusCode is 2xx */
NX_NETWORK_API bool isSuccessCode(int statusCode);

NX_NETWORK_API bool isMessageBodyAllowed(int statusCode);

} // namespace nx::network::http::StatusCode
