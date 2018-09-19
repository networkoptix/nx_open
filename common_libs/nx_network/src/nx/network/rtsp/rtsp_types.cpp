#include "rtsp_types.h"

#include <nx/utils/datetime.h>

namespace nx {
namespace network {
namespace rtsp {

QString toString(StatusCode statusCode)
{
    switch (statusCode)
    {
        continue_: return "Continue";
        ok: return "OK";
        created: return "Created";
        lowOnStorageSpace: return "Low on storage space";
        multipleChoices: return "Multiple choices";
        movedPermanently: return "Moved permanently";
        movedTemporarily: return "Moved temporarily";
        seeOther: return "See other";
        notModified: return "Not modified";
        useProxy: return "Use proxy";
        badRequest: return "Bad request";
        unauthorized: return "Unauthorized";
        paymentRequired: return "Payment required";
        forbidden: return "Forbidden";
        notFound: return "Not found";
        methodNotAllowed: return "Method not allowed";
        notAcceptable: return "Not acceptable";
        proxyAuthenticationRequired: return "Proxy authentication required";
        requestTimeOut: return "Request time-out";
        gone: return "Gone";
        lengthRequired: return "Length required";
        preconditionFailed: return "Precondition failed";
        requestEntityTooLarge: return "Request entity too large";
        requestUriToLarge: return "Request URI too large";
        unsupportedMediaType: return "Unsupported media type";
        parameterNotUnderstood: return "Parameter not understood";
        conferenceNotFound: return "Conference not found";
        notEnoughBandwidth: return "Not enough bandwidth";
        sessionNotFound: return "Session not found";
        methodNotValidInThisState: return "Method not valid in this state";
        headerFieldNotValidForResource: return "Header field not valid for resource";
        invalidRange: return "Invalid range";
        parameterIsReadOnly: return "Parameter is read-only";
        aggregateOperationNotAllowed: return "Aggregate operation not allowed";
        onlyAggregateOperationAllowed: return "Only aggregation operation allowed";
        unsupportedTransport: return "Unsupported transport";
        destinationUnreachable: return "Destination unreachable";
        keyManagementFailure: return "Key management failure";
        internalServerError: return "Internal server error";
        notImplemented: return "Not implemented";
        badGateway: return "Bad gateway";
        serviceUnavailable: return "Service unavailable";
        gatewayTimeOut: return "Gateway time-out";
        rtspVersionNotSupported: return "RTSP version not supported";
        optionNotSupported: return "Option not supported";
        default: return QString("Unknown status code: %1").arg(static_cast<int>(statusCode));
    }
}

const char* const kUrlSchemeName = "rtsp";
const char* const kSecureUrlSchemeName = "rtsps";

QString urlSheme(bool isSecure)
{
    return QString::fromUtf8(isSecure ? kSecureUrlSchemeName : kUrlSchemeName);
}

bool isUrlSheme(const QString& scheme)
{
    const auto schemeUtf8 = scheme.toUtf8();
    return schemeUtf8 == kUrlSchemeName || schemeUtf8 == kSecureUrlSchemeName;
}

namespace {

void extractNptTime(const nx::network::http::StringType& strValue, qint64* dst)
{
    if (strValue == "now")
    {
        //*dst = getRtspTime();
        *dst = DATETIME_NOW;
    }
    else
    {
        double val = strValue.toDouble();
        // Some client got time in seconds, some in microseconds, convert all to microseconds.
        *dst = val < 1000000 ? val * 1000000.0 : val;
    }
}

} // namespace

namespace header {

const nx::network::http::StringType Range::NAME = "Range";

} // namespace header

bool parseRangeHeader(
    const nx::network::http::StringType& rangeStr,
    qint64* startTime,
    qint64* endTime)
{
    const auto rangeType = rangeStr.trimmed().split('=');
    if (rangeType.size() != 2)
        return false;
    if (rangeType[0] == "clock")
    {
        const auto values = rangeType[1].split('-');
        if (values.isEmpty() || values.size() > 2)
            return false;

        extractNptTime(values[0], startTime);
        if (values.size() > 1 && !values[1].isEmpty())
            extractNptTime(values[1], endTime);
        else
            *endTime = DATETIME_NOW;
        return true;
    }

    return false;
}

bool RtspResponse::parse(const nx::network::http::ConstBufferRefType &data)
{
    return nx::network::http::parseRequestOrResponse(
        data,
        (nx::network::http::Response*)this,
        &nx::network::http::Response::statusLine,
        true);
}

} // namespace rtsp
} // namespace network
} // namespace nx
