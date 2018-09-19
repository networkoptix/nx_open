#include "rtsp_types.h"

#include <nx/utils/datetime.h>

namespace nx {
namespace network {
namespace rtsp {

QString toString(StatusCode statusCode)
{
    switch (statusCode)
    {
        case StatusCode::continue_: return "Continue";
        case StatusCode::ok: return "OK";
        case StatusCode::created: return "Created";
        case StatusCode::lowOnStorageSpace: return "Low on storage space";
        case StatusCode::multipleChoices: return "Multiple choices";
        case StatusCode::movedPermanently: return "Moved permanently";
        case StatusCode::movedTemporarily: return "Moved temporarily";
        case StatusCode::seeOther: return "See other";
        case StatusCode::notModified: return "Not modified";
        case StatusCode::useProxy: return "Use proxy";
        case StatusCode::badRequest: return "Bad request";
        case StatusCode::unauthorized: return "Unauthorized";
        case StatusCode::paymentRequired: return "Payment required";
        case StatusCode::forbidden: return "Forbidden";
        case StatusCode::notFound: return "Not found";
        case StatusCode::methodNotAllowed: return "Method not allowed";
        case StatusCode::notAcceptable: return "Not acceptable";
        case StatusCode::proxyAuthenticationRequired: return "Proxy authentication required";
        case StatusCode::requestTimeOut: return "Request time-out";
        case StatusCode::gone: return "Gone";
        case StatusCode::lengthRequired: return "Length required";
        case StatusCode::preconditionFailed: return "Precondition failed";
        case StatusCode::requestEntityTooLarge: return "Request entity too large";
        case StatusCode::requestUriToLarge: return "Request URI too large";
        case StatusCode::unsupportedMediaType: return "Unsupported media type";
        case StatusCode::parameterNotUnderstood: return "Parameter not understood";
        case StatusCode::conferenceNotFound: return "Conference not found";
        case StatusCode::notEnoughBandwidth: return "Not enough bandwidth";
        case StatusCode::sessionNotFound: return "Session not found";
        case StatusCode::methodNotValidInThisState: return "Method not valid in this state";
        case StatusCode::headerFieldNotValidForResource: return "Header field not valid for resource";
        case StatusCode::invalidRange: return "Invalid range";
        case StatusCode::parameterIsReadOnly: return "Parameter is read-only";
        case StatusCode::aggregateOperationNotAllowed: return "Aggregate operation not allowed";
        case StatusCode::onlyAggregateOperationAllowed: return "Only aggregation operation allowed";
        case StatusCode::unsupportedTransport: return "Unsupported transport";
        case StatusCode::destinationUnreachable: return "Destination unreachable";
        case StatusCode::keyManagementFailure: return "Key management failure";
        case StatusCode::internalServerError: return "Internal server error";
        case StatusCode::notImplemented: return "Not implemented";
        case StatusCode::badGateway: return "Bad gateway";
        case StatusCode::serviceUnavailable: return "Service unavailable";
        case StatusCode::gatewayTimeOut: return "Gateway time-out";
        case StatusCode::rtspVersionNotSupported: return "RTSP version not supported";
        case StatusCode::optionNotSupported: return "Option not supported";
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
