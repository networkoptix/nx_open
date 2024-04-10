// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_status.h"

#include <nx/utils/std_string_utils.h>

namespace nx::network::http::StatusCode {

std::string toString(int val)
{
    return toString(Value(val));
}

std::string toString(Value val)
{
    switch (val)
    {
        case undefined:
            break;

        // 1XX
        case _continue:
            return "Continue";
        case switchingProtocols:
            return "Switching Protocols";

        // 2XX
        case ok:
            return "OK";
        case created:
            return "Created";
        case accepted:
            return "Accepted";
        case noContent:
            return "No Content";
        case partialContent:
            return "Partial Content";
        case multiStatus:
            return "Multi-Status";
        case multipleChoices:
            return "Multiple Choices";
        case movedPermanently:
            return "Moved Permanently";
        case found:
            return "Found";
        case seeOther:
            return "See Other";
        case notModified:
            return "Not Modified";
        case useProxy:
            return "Use Proxy";
        case temporaryRedirect:
            return "Temporary Redirect";
        case permanentRedirect:
            return "Permanent Redirect";
        case lastSuccessCode:
            break;

        // 4XX
        case badRequest:
            return "Bad Request";
        case unauthorized:
            return "Unauthorized";
        case paymentRequired:
            return "Payment Required";
        case forbidden:
            return "Forbidden";
        case notFound:
            return "Not Found";
        case notAllowed:
            return "Not Allowed";
        case notAcceptable:
            return "Not Acceptable";
        case proxyAuthenticationRequired:
            return "Proxy Authentication Required";
        case requestTimeOut:
            return "Request Timeout";
        case conflict:
            return "Conflict";
        case gone:
            return "Gone";
        case lengthRequired:
            return "Length Required";
        case preconditionFailed:
            return "Precondition Failed";
        case requestEntityTooLarge:
            return "Request Entity Too Large";
        case requestUriToLarge:
            return "Request Uri To Large";
        case rangeNotSatisfiable:
            return "Range Not Satisfiable";
        case unsupportedMediaType:
            return "Unsupported Media Type";
        case unprocessableEntity:
            return "Unprocessable Entity";
        case tooManyRequests:
            return "Too Many Requests";
        case unavailableForLegalReasons:
            return "Unavailable For Legal Reasons";

        // 5XX
        case internalServerError:
            return "Internal Server Error";
        case notImplemented:
            return "Not Implemented";
        case badGateway:
            return "Bad Gateway";
        case serviceUnavailable:
            return "Service Unavailable";
        case gatewayTimeOut:
            return "Gateway Timeout";
    }
    return nx::utils::buildString("Unknown_", (int) val);
}

bool isSuccessCode(Value statusCode)
{
    return isSuccessCode(static_cast<int>(statusCode));
}

bool isSuccessCode(int statusCode)
{
    return (statusCode >= ok && statusCode <= lastSuccessCode) ||
            statusCode == switchingProtocols;
}

bool isMessageBodyAllowed(int statusCode)
{
    switch (statusCode)
    {
        case noContent:
        case notModified:
            return false;

        default:
            // Message body is forbidden for informational status codes.
            if (statusCode / 100 == 1)
                return false;
            return true;
    }
}

} // namespace nx::network::http::StatusCode
