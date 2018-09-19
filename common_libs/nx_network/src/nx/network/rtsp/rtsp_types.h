#pragma once

#include <nx/network/http/http_types.h>

namespace nx {
namespace network {
namespace rtsp {

enum class StatusCode
{
    continue_ = 100,
    ok = 200,
    created = 201,
    lowOnStorageSpace = 250,
    multipleChoices = 300,
    movedPermanently = 301,
    movedTemporarily = 302,
    seeOther = 303,
    notModified = 304,
    useProxy = 305,
    badRequest = 400,
    unauthorized = 401,
    paymentRequired = 402,
    forbidden = 403,
    notFound = 404,
    methodNotAllowed = 405,
    notAcceptable = 406,
    proxyAuthenticationRequired = 407,
    requestTimeOut = 408,
    gone = 410,
    lengthRequired = 411,
    preconditionFailed = 412,
    requestEntityTooLarge = 413,
    requestUriToLarge = 414,
    unsupportedMediaType = 415,
    parameterNotUnderstood = 451,
    conferenceNotFound = 452,
    notEnoughBandwidth = 453,
    sessionNotFound = 454,
    methodNotValidInThisState = 455,
    headerFieldNotValidForResource = 456,
    invalidRange = 457,
    parameterIsReadOnly = 458,
    aggregateOperationNotAllowed = 459,
    onlyAggregateOperationAllowed = 460,
    unsupportedTransport = 461,
    destinationUnreachable = 462,
    keyManagementFailure = 463,
    internalServerError = 500,
    notImplemented = 501,
    badGateway = 502,
    serviceUnavailable = 503,
    gatewayTimeOut = 504,
    rtspVersionNotSupported = 505,
    optionNotSupported = 551,
};

NX_NETWORK_API QString toString(StatusCode statusCode);

namespace header {

class NX_NETWORK_API Range
{
public:
    static const nx::network::http::StringType NAME;
};

} // namespace header

NX_NETWORK_API extern const char* const kUrlSchemeName;
NX_NETWORK_API extern const char* const kSecureUrlSchemeName;

QString NX_NETWORK_API urlSheme(bool isSecure);
bool NX_NETWORK_API isUrlSheme(const QString& scheme);

const int DEFAULT_RTSP_PORT = 554;

static const nx::network::http::MimeProtoVersion rtsp_1_0 = { "RTSP", "1.0" };

class NX_NETWORK_API RtspResponse: public nx::network::http::Response
{
public:
    bool parse(const nx::network::http::ConstBufferRefType& data);
};

/**
 * Parses Range header ([rfc2326, 12.29]).
 * Returns range in microseconds.
 * NOTE: Only clock is supported.
 *   Though, it MUST contain UTC timestamp (millis or usec). I.e., not rfc2326-compliant.
 * NOTE: For now constant DATETIME_NOW is returned.
 * @return true if startTime and endTime were filled with values.
*/
bool NX_NETWORK_API parseRangeHeader(
    const nx::network::http::StringType& rangeStr,
    qint64* startTime, qint64* endTime);

} // namespace rtsp
} // namespace network
} // namespace nx
