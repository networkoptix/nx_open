// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_types.h"

#include <cstring>
#include <memory>

#include <stdio.h>

#include <QtCore/QLocale>

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/socket_common.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/switch.h>

namespace nx::network::http {

namespace {

// Sun, 06 Nov 1994 08:49:37 GMT (RFC 822, updated by RFC 1123)
static QDateTime parseRfc1123Date(const std::string_view& str)
{
    static constexpr char kRfc1123Template[] = "ddd, dd MMM yyyy hh:mm:ss";
    return QLocale(QLocale::C).toDateTime(
        QByteArray::fromRawData(str.data(), str.size()), kRfc1123Template);
}

// Sunday, 06-Nov-94 08:49:37 GMT (RFC 850, obsoleted by RFC 1036)
static QDateTime parseRfc850Date(const std::string_view& str)
{
    static constexpr char kRfc850Template[] = "dddd, dd-MMM-yy hh:mm:ss";
    return QLocale(QLocale::C).toDateTime(
        QByteArray::fromRawData(str.data(), str.size()), kRfc850Template);
}

// Sun Nov  6 08:49:37 1994  (ANSI C's asctime() format)
static QDateTime parseAnsiCDate(const std::string_view& str)
{
    // QDateTime::fromString() requires exact format match, so two templates are required:
    // Non padded if the day of the month is >= 10, and padded if day of the month is < 10
    static constexpr char kAnsiCTemplate[]       = "ddd MMM d hh:mm:ss yyyy";
    static constexpr char kAnsiCTemplatePadded[] = "ddd MMM  d hh:mm:ss yyyy";

    if (str.size() < 9)
        return QDateTime();

    return QLocale(QLocale::C).toDateTime(
        QByteArray::fromRawData(str.data(), str.size()),
        str[8] == ' ' ? kAnsiCTemplatePadded : kAnsiCTemplate);
}

} //namespace

const char* urlScheme(bool isSecure)
{
    return isSecure ? kSecureUrlSchemeName : kUrlSchemeName;
}

bool isUrlScheme(const std::string_view& scheme)
{
    return nx::utils::stricmp(scheme, kUrlSchemeName) == 0
        || nx::utils::stricmp(scheme, kSecureUrlSchemeName) == 0;
}

int defaultPortForScheme(const std::string_view& scheme)
{
    if (nx::utils::stricmp(scheme, kUrlSchemeName) == 0)
        return DEFAULT_HTTP_PORT;
    if (nx::utils::stricmp(scheme, kSecureUrlSchemeName) == 0)
        return DEFAULT_HTTPS_PORT;
    return -1;
}

int defaultPort(bool isSecure)
{
    return isSecure ? DEFAULT_HTTPS_PORT : DEFAULT_HTTP_PORT;
}

std::string getHeaderValue(
    const HttpHeaders& headers,
    const std::string_view& headerName)
{
    auto it = headers.lower_bound(headerName);
    return it == headers.end() || nx::utils::stricmp(it->first, headerName) != 0
        ? std::string()
        : it->second;
}

bool readHeader(
    const HttpHeaders& headers,
    const std::string_view& headerName,
    int* value)
{
    auto it = headers.lower_bound(headerName);
    if (it == headers.end() || nx::utils::stricmp(it->first, headerName) != 0)
        return false;

    *value = nx::utils::stoi(it->second);
    return true;
}

HttpHeaders::iterator insertOrReplaceHeader(HttpHeaders* const headers, const HttpHeader& newHeader)
{
    auto existingHeaderIter = headers->lower_bound(newHeader.first);
    if ((existingHeaderIter != headers->end()) &&
        (nx::utils::stricmp(existingHeaderIter->first, newHeader.first) == 0))
    {
        existingHeaderIter->second = newHeader.second;  //replacing header
        return existingHeaderIter;
    }
    else
    {
        return headers->insert(existingHeaderIter, newHeader);
    }
}

HttpHeaders::iterator insertHeader(HttpHeaders* const headers, HttpHeader newHeader)
{
    return headers->emplace(std::move(newHeader));
}

void removeHeader(HttpHeaders* const headers, const std::string& name)
{
    headers->erase(name);
}

static bool hasOrigin(const std::string& origins, const std::string& origin)
{
    std::vector<std::string_view> tokens;
    nx::utils::split(origins, ',', std::back_inserter(tokens));
    return std::find(tokens.begin(), tokens.end(), origin) != tokens.end();
}

void insertOrReplaceCorsHeaders(
    HttpHeaders* headers,
    const Method& method,
    std::string origin,
    const std::string& supportedOrigins,
    std::string_view methods)
{
    if (!origin.empty() && (supportedOrigins == "*" || hasOrigin(supportedOrigins, origin)))
    {
        insertOrReplaceHeader(headers, {"Access-Control-Allow-Credentials", "true"});
        insertOrReplaceHeader(headers, {"Access-Control-Allow-Origin", std::move(origin)});
    }
    else if (!supportedOrigins.empty() && supportedOrigins.find(',') == std::string::npos)
    {
        insertOrReplaceHeader(headers, {"Access-Control-Allow-Origin", supportedOrigins});
    }
    if (method == Method::get)
        return;

    insertOrReplaceHeader(headers, HttpHeader("Access-Control-Allow-Methods", std::move(methods)));
    insertOrReplaceHeader(
        headers, HttpHeader("Access-Control-Allow-Headers", "X-PINGOTHER, Content-Type"));
    insertOrReplaceHeader(headers, HttpHeader("Access-Control-Max-Age", "600"));
    insertOrReplaceHeader(headers, HttpHeader("Vary", "Accept-Encoding, Origin"));
}

//-------------------------------------------------------------------------------------------------
// parse utils

static size_t estimateSerializedDataSize(const HttpHeaders& headers)
{
    size_t requiredSize = 0;
    for (HttpHeaders::const_iterator
        it = headers.begin();
        it != headers.end();
        ++it)
    {
        requiredSize += it->first.size() + 1 + it->second.size() + 2;
    }
    return requiredSize;
}

void serializeHeaders(const HttpHeaders& headers, nx::Buffer* dstBuffer)
{
    nx::utils::join(
        dstBuffer,
        headers.begin(), headers.end(),
        "",
        [](auto* out, const auto& header)
        {
            nx::utils::buildString(out, header.first, ": ", header.second, "\r\n");
        });
}

bool parseHeader(
    const ConstBufferRefType& data,
    std::string* const name,
    std::string* const value)
{
    ConstBufferRefType nameRef;
    ConstBufferRefType valueRef;
    if (!parseHeader(data, &nameRef, &valueRef))
        return false;

    *name = nameRef;
    *value = valueRef;
    return true;
}

bool parseHeader(
    const ConstBufferRefType& data,
    ConstBufferRefType* const name,
    ConstBufferRefType* const value)
{
    const auto trimmedData = nx::utils::trim(data);
    const auto separatorPos = trimmedData.find(':');
    if (separatorPos == trimmedData.npos)
        return false;

    *name = nx::utils::trim(trimmedData.substr(0, separatorPos));
    *value = nx::utils::trim(trimmedData.substr(separatorPos+1));

    return true;
}

HttpHeader parseHeader(const ConstBufferRefType& data)
{
    ConstBufferRefType name;
    ConstBufferRefType value;
    parseHeader(data, &name, &value);
    return HttpHeader(name, value);
}

//-------------------------------------------------------------------------------------------------
// StatusCode.

namespace StatusCode {

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

} // namespace StatusCode

//-------------------------------------------------------------------------------------------------
// class Method.

Method::Method(const std::string_view& str):
    m_value(nx::utils::toUpper(str))
{
}

Method::Method(const char* str):
    Method(std::string_view(str))
{
}

Method::Method(std::string str):
    m_value(std::move(str))
{
}

Method& Method::operator=(const std::string_view& str)
{
    m_value = nx::utils::toUpper(str);
    return *this;
}

Method& Method::operator=(const char* str)
{
    m_value = nx::utils::toUpper(std::string_view(str));
    return *this;
}

Method& Method::operator=(std::string str)
{
    m_value = nx::utils::toUpper(std::move(str));
    return *this;
}

bool Method::operator<(const Method& right) const
{
    return nx::utils::stricmp(m_value, right.m_value) < 0;
}

bool Method::operator<(const std::string_view& right) const
{
    return nx::utils::stricmp(m_value, right) < 0;
}

bool Method::operator==(const Method& right) const
{
    return nx::utils::stricmp(m_value, right.m_value) == 0;
}

bool Method::operator!=(const Method& right) const
{
    return nx::utils::stricmp(m_value, right.m_value) != 0;
}

const std::string& Method::toString() const
{
    return m_value;
}

bool Method::isKnown(const std::string_view& str)
{
    return nx::utils::switch_(str,
        get, []() { return true; },
        post, []() { return true; },
        put, []() { return true; },
        patch, []() { return true; },
        delete_, []() { return true; },
        connect, []() { return true; },
        head, []() { return true; },
        options, []() { return true; },
        nx::utils::default_, []() { return false; });
}

bool Method::isMessageBodyAllowed(const Method& method)
{
    return method != get && method != head && method != delete_ && method != connect;
}

bool Method::isMessageBodyAllowedInResponse(
    const Method& method,
    StatusCode::Value statusCode)
{
    return method != connect
        && method != head
        && StatusCode::isMessageBodyAllowed(statusCode);
}

//-------------------------------------------------------------------------------------------------
// class MimeProtoVersion.

static size_t estimateSerializedDataSize(const MimeProtoVersion& val)
{
    return val.protocol.size() + 1 + val.version.size();
}

bool MimeProtoVersion::parse(const ConstBufferRefType& data)
{
    // TODO split.

    protocol.clear();
    version.clear();

    const auto sepPos = data.find('/');
    if (sepPos == ConstBufferRefType::npos)
        return false;

    protocol.append(data.data(), sepPos);
    version.append(data.data() + sepPos + 1, data.size() - (sepPos + 1));
    return true;
}

void MimeProtoVersion::serialize(nx::Buffer* const dstBuffer) const
{
    nx::utils::buildString(dstBuffer, protocol, "/", version);
}


//-------------------------------------------------------------------------------------------------
// class RequestLine.

static size_t estimateSerializedDataSize(const RequestLine& rl)
{
    return rl.method.toString().size() + 1 + rl.url.toString().size() + 1 + estimateSerializedDataSize(rl.version) + 2;
}

bool RequestLine::parse(const ConstBufferRefType& data)
{
    // TODO: split

    enum ParsingState
    {
        psMethod,
        psUrl,
        psVersion,
        psDone
    }
    parsingState = psMethod;

    #if defined(_WIN32)
        // This function may crash in random places (like in VMS-18435). Copy request line to the
        // stack so it is present in the mini-dump.
        char urlOnStack[256] = {0};
        snprintf(urlOnStack, sizeof(urlOnStack), "%s", std::string(data.data(), data.size()).c_str());
    #endif

    const char* str = data.data();
    const char* strEnd = str + data.size();
    const char* tokenStart = nullptr;
    bool waitingNextToken = true;
    for (; str <= strEnd; ++str)
    {
        if ((*str == ' ') || (str == strEnd))
        {
            if (!waitingNextToken) //waiting end of token
            {
                //found new token [tokenStart, str)
                switch (parsingState)
                {
                    case psMethod:
                        method = std::string_view(tokenStart, str - tokenStart);
                        parsingState = psUrl;
                        break;

                    case psUrl:
                    {
                        const auto urlStr = QByteArray(tokenStart, str - tokenStart);
                        // RFC 2616, section-5.1.2: "The authority form is only used by the CONNECT
                        // method (section 9.9)."
                        if (method == Method::connect)
                            url.setAuthority(urlStr, QUrl::StrictMode);
                        else
                            url.setUrl(urlStr);
                        parsingState = psVersion;
                        break;
                    }

                    case psVersion:
                        version.parse(data.substr(tokenStart - data.data(), str - tokenStart));
                        parsingState = psDone;
                        break;

                    default:
                        return false;
                }

                waitingNextToken = true;
            }
        }
        else
        {
            if (waitingNextToken)
            {
                tokenStart = str;
                waitingNextToken = false;   //waiting token end
            }
        }
    }

    return parsingState == psDone;
}

void RequestLine::serialize(nx::Buffer* const dstBuffer) const
{
    *dstBuffer += method.toString();
    *dstBuffer += " ";
    const auto path = encodeUrl(
        url, (method == Method::connect) ? EncodeUrlParts::authority : EncodeUrlParts::all);
    *dstBuffer += path.empty() ? "/" : path;
    *dstBuffer += " ";
    version.serialize(dstBuffer);
    *dstBuffer += "\r\n";
}

std::string RequestLine::toString() const
{
    nx::Buffer buf;
    serialize(&buf);
    return std::string(nx::utils::trim((std::string_view) buf));
}

std::string RequestLine::encodeUrl(const nx::utils::Url& url, EncodeUrlParts parts) const
{
    QString encoded;
    switch (parts)
    {
        case EncodeUrlParts::all:
            encoded =
                url.toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters);
            break;
        case EncodeUrlParts::authority:
            encoded = url.authority();
            break;
    };

    // Encoding '+' to '%2B' since there are buggy HTTP servers out there that decode '+' as space
    // which is unexpected and undesirable.
    if (version.protocol == "HTTP")
        return nx::utils::replace(encoded.toStdString(), "\\+", "%2B");
    return encoded.toStdString();
}

//-------------------------------------------------------------------------------------------------
// class StatusLine.

static const int MAX_DIGITS_IN_STATUS_CODE = 5;
static size_t estimateSerializedDataSize(const StatusLine& sl)
{
    return estimateSerializedDataSize(sl.version)
        + 1
        + MAX_DIGITS_IN_STATUS_CODE
        + 1
        + sl.reasonPhrase.size()
        + 2;
}

StatusLine::StatusLine():
    statusCode(StatusCode::undefined)
{
}

bool StatusLine::parse(const ConstBufferRefType& data)
{
    // TODO: split

    const size_t versionStart = 0;
    const auto versionEnd = data.find_first_of(" ", versionStart);
    if (versionEnd == ConstBufferRefType::npos)
        return false;
    if (!version.parse(data.substr(versionStart, versionEnd - versionStart)))
        return false;

    const auto statusCodeStart = data.find_first_not_of(" ", versionEnd);
    if (statusCodeStart == ConstBufferRefType::npos)
        return false;
    const auto statusCodeEnd = data.find_first_of(" ", statusCodeStart);
    if (statusCodeEnd == ConstBufferRefType::npos)
        return false;
    statusCode = static_cast<StatusCode::Value>(nx::utils::stoul(data.substr(
        statusCodeStart,
        statusCodeEnd - statusCodeStart)));

    const auto reasonPhraseStart = data.find_first_not_of(" ", statusCodeEnd);
    if (reasonPhraseStart == ConstBufferRefType::npos)
        return false;
    const auto reasonPhraseEnd = data.find_first_of("\r\n", reasonPhraseStart);
    reasonPhrase = data.substr(
        reasonPhraseStart,
        reasonPhraseEnd == ConstBufferRefType::npos
            ? ConstBufferRefType::npos
            : reasonPhraseEnd - reasonPhraseStart);

    return true;
}

void StatusLine::serialize(nx::Buffer* dstBuffer) const
{
    version.serialize(dstBuffer);

    nx::utils::buildString(
        dstBuffer,
        " ", (int) statusCode, " ", reasonPhrase, "\r\n");
}

std::string StatusLine::toString() const
{
    nx::Buffer buf;
    serialize(&buf);
    return toStdString(std::move(buf));
}

//-------------------------------------------------------------------------------------------------
// class Request.

bool Request::parse(const ConstBufferRefType& data)
{
    return parseRequestOrResponse(data, this, &Request::requestLine);
}

void Request::serialize(nx::Buffer* dstBuffer) const
{
    //estimating required buffer size
    dstBuffer->reserve(
        dstBuffer->size() +
        estimateSerializedDataSize(requestLine) +
        estimateSerializedDataSize(headers) +
        2 +
        messageBody.size());

    //serializing
    requestLine.serialize(dstBuffer);
    serializeHeaders(headers, dstBuffer);
    dstBuffer->append("\r\n");
    dstBuffer->append(messageBody);
}

nx::Buffer Request::serialized() const
{
    nx::Buffer buf;
    serialize(&buf);
    return buf;
}

std::string Request::toString() const
{
    return toStdString(serialized());
}

std::string Request::getCookieValue(const std::string_view& name) const
{
    auto cookieIter = headers.find("cookie");
    if (cookieIter == headers.end())
        return std::string();

    auto splitter = nx::utils::makeSplitter(
        cookieIter->second, nx::utils::separator::isAnyOf(';'));
    while (splitter.next())
    {
        const auto [tokens, count] =
            nx::utils::split_n<2>(splitter.token(), nx::utils::separator::isAnyOf('='));
        if (count > 1 && nx::utils::trim(tokens[0]) == name && tokens[1] != kDeletedCookieValue)
            return std::string(tokens[1]);
    }

    return std::string();
}

void Request::removeCookie(const std::string_view& name)
{
    static constexpr auto kCookieHeader = "Cookie";

    const auto cookie = getHeaderValue(headers, kCookieHeader);
    if (cookie.empty())
        return;

    std::vector<std::pair<std::string_view, std::string_view>> params;
    nx::utils::splitNameValuePairs(cookie, ';', '=', std::back_inserter(params));

    nx::utils::erase_if(
        params,
        [&name](const auto& nameValue) { return nameValue.first == name; });

    std::string newCookie;
    newCookie.reserve(cookie.size());

    nx::utils::join(
        &newCookie,
        params,
        "; ", //< Space is mandatory here according to the specification.
        [](std::string* out, const auto& nameValue)
        {
            return nx::utils::buildString(out, nameValue.first, '=', nameValue.second);
        });

    if (newCookie.empty())
        nx::network::http::removeHeader(&headers, kCookieHeader);
    else
        insertOrReplaceHeader(&headers, HttpHeader(kCookieHeader, newCookie));
}

//-------------------------------------------------------------------------------------------------
// class Response.

bool Response::parse(const ConstBufferRefType& data)
{
    return parseRequestOrResponse(data, this, &Response::statusLine);
}

void Response::serialize(nx::Buffer* dstBuffer) const
{
    //estimating required buffer size
    dstBuffer->reserve(
        dstBuffer->size() +
        estimateSerializedDataSize(statusLine) +
        estimateSerializedDataSize(headers) +
        2 +
        messageBody.size());

    //serializing
    statusLine.serialize(dstBuffer);
    serializeHeaders(headers, dstBuffer);
    dstBuffer->append("\r\n");
    dstBuffer->append(messageBody);
}

void Response::serializeMultipartResponse(
    const ConstBufferRefType& boundary,
    nx::Buffer* dstBuffer) const
{
    //estimating required buffer size
    dstBuffer->reserve(
        dstBuffer->size() +
        boundary.size() +
        2 +
        estimateSerializedDataSize(headers) +
        2 +
        messageBody.size() +
        2);

    //serializing
    nx::utils::buildString(dstBuffer, boundary, "\r\n");
    serializeHeaders(headers, dstBuffer);
    nx::utils::buildString(dstBuffer, "\r\n", (std::string_view) messageBody, "\r\n");
}

std::string Response::toString() const
{
    nx::Buffer buf;
    serialize(&buf);
    return toStdString(std::move(buf));
}

std::string Response::toMultipartString(const ConstBufferRefType& boundary) const
{
    nx::Buffer buf;
    serializeMultipartResponse(boundary, &buf);
    return toStdString(std::move(buf));
}

static constexpr char kSetCookieHeaderName[] = "Set-Cookie";

HttpHeader deletedCookieHeader(const std::string& name, const std::string& path)
{
    return {kSetCookieHeaderName,
        name + "=" + kDeletedCookieValue + "; Path=" + path
            + "; expires=Thu, 01 Jan 1970 00:00 : 00 GMT"};
}

HttpHeader cookieHeader(const std::string& name,
    const std::string& value,
    const std::string& path,
    bool secure,
    bool httpOnly)
{
    // Using SameSite=None if cookie is secure to mimic old browser behavior, see:
    //     https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Set-Cookie/SameSite
    return {kSetCookieHeaderName,
        name + "=" + value + "; Path=" + path + (secure ? "; SameSite=None; Secure" : "")
        + (httpOnly ? "; HttpOnly" : "")};
}

void Response::setCookie(
    const std::string& name, const std::string& value,
    const std::string& path, bool secure)
{
    insertHeader(&headers, cookieHeader(name, value, path, secure));
}

void Response::setDeletedCookie(const std::string& name)
{
    insertHeader(&headers, deletedCookieHeader(name));
}

std::map<std::string, std::string> Response::getCookies() const
{
    std::map<std::string, std::string> cookies;
    const auto setCookieHeaders = headers.equal_range("Set-Cookie");
    for (auto it = setCookieHeaders.first; it != setCookieHeaders.second; ++it)
    {
        const auto& data = it->second;
        const auto nameEnd = data.find('=');
        if (nameEnd == data.npos)
            continue;

        const auto valueBegin = nameEnd + 1;
        auto valueEnd = data.find_first_of("; ", valueBegin);
        if (valueEnd == data.npos)
            valueEnd = data.size();

        cookies.emplace(
            data.substr(0, nameEnd),
            data.substr(valueBegin, valueEnd - valueBegin));
    }

    return cookies;
}

namespace MessageType {

const char* toString(Value val)
{
    switch (val)
    {
        case request:
            return "request";
        case response:
            return "response";
        default:
            return "unknown";
    }
}

} // namespace MessageType

static bool isMessageBodyForbiddenByHeaders(const HttpHeaders& headers)
{
    auto contentLengthIter = headers.find("Content-Length");
    if (contentLengthIter != headers.end() &&
        nx::utils::stoull(contentLengthIter->second) == 0)
    {
        return true;
    }

    return false;
}

bool isMessageBodyPresent(const Response& response)
{
    if (!StatusCode::isMessageBodyAllowed(response.statusLine.statusCode))
        return false;

    if (isMessageBodyForbiddenByHeaders(response.headers))
        return false;

    return true;
}


//-------------------------------------------------------------------------------------------------
// class Message.

Message::Message(MessageType::Value _type):
    type(_type)
{
    if (type == MessageType::request)
        request = new Request();
    else if (type == MessageType::response)
        response = new Response();
}

Message::Message(const Message& right):
    type(right.type)
{
    if (type == MessageType::request)
        request = new Request(*right.request);
    else if (type == MessageType::response)
        response = new Response(*right.response);
    else
        request = NULL;
}

Message::Message(Message&& right):
    type(right.type),
    request(right.request)
{
    right.type = MessageType::none;
    right.request = nullptr;
}

Message::~Message()
{
    clear();
}

Message& Message::operator=(const Message& right)
{
    clear();
    type = right.type;
    if (type == MessageType::request)
        request = new Request(*right.request);
    else if (type == MessageType::response)
        response = new Response(*right.response);
    return *this;
}

Message& Message::operator=(Message&& right)
{
    clear();

    type = right.type;
    right.type = MessageType::none;
    request = right.request;
    right.request = nullptr;
    return *this;
}

void Message::serialize(nx::Buffer* dstBuffer) const
{
    switch (type)
    {
        case MessageType::request:
            return request->serialize(dstBuffer);
        case MessageType::response:
            return response->serialize(dstBuffer);
        default:
            return /*false*/;
    }
}

void Message::clear()
{
    if (type == MessageType::request)
        delete request;
    else if (type == MessageType::response)
        delete response;
    request = NULL;
    type = MessageType::none;
}

void Message::setBody(nx::Buffer body)
{
    if (type == MessageType::request)
        request->messageBody = std::move(body);
    else if (type == MessageType::response)
        response->messageBody = std::move(body);
}

std::string Message::toString() const
{
    nx::Buffer str;
    serialize(&str);
    return toStdString(std::move(str));
}

bool Message::operator==(const Message& msg) const
{
    if (type != msg.type)
        return false;

    switch (type)
    {
        case MessageType::request: return *request == *msg.request;
        case MessageType::response: return *response == *msg.response;
        default: return true;
    }
}

namespace header {

namespace AuthScheme {

std::string_view toString(Value val)
{
    switch (val)
    {
        case none:
            return "None";
        case basic:
            return "Basic";
        case digest:
            return "Digest";
        case bearer:
            return "Bearer";
    }

    NX_ASSERT(false, "Invalid value: %1", static_cast<int>(val));
    return "InvalidValue";
}

Value fromString(const std::string_view& str)
{
    if (str == "Basic")
        return basic;
    if (str == "Digest")
        return digest;
    if (str == "Bearer")
        return bearer;
    return none;
}

} // namespace AuthScheme

//-------------------------------------------------------------------------------------------------
// Authorization.

bool BasicCredentials::parse(const std::string_view& base64Str)
{
    const auto str = nx::utils::fromBase64(base64Str);

    const auto [tokens, count] = nx::utils::split_n<2>(str, ':');
    if (count < 2)
        return false;

    userid = tokens[0];
    password = tokens[1];

    return true;
}

void BasicCredentials::serialize(nx::Buffer* dest) const
{
    *dest += nx::utils::toBase64(nx::utils::buildString(userid, ':', password));
}

bool DigestCredentials::parse(const std::string_view& str, char separator)
{
    nx::utils::splitNameValuePairs(
        str, separator, '=', std::inserter(params, params.end()));

    auto usernameIter = params.find("username");
    if (usernameIter != params.cend())
        userid = usernameIter->second;
    return true;
}

void DigestCredentials::serialize(nx::Buffer* dest) const
{
    const static std::array<const char*, 5> predefinedOrder =
    {{
        "username",
        "realm",
        "nonce",
        "uri",
        "response"
    }};

    // TODO: #akolesnikov Re-write this function using nx::utils::join.

    auto needQuoteForParam = [](const auto& value)
    {
        // According to RFC ( https://httpwg.org/specs/rfc7616.html ) fields 'algorithm', 'qop', and 'nc' must not be quoted.
        // But ffmpeg and others ignore RFC and use quotes for 'algorithm' and 'qop'
        // Comment for ffmpeg: <we are violating the RFC and use "" because all others seem to do that too.>
        if (value == "nc")
            return false;
        return true;
    };

    bool isFirst = true;
    auto serializeParam =
        [&isFirst, dest](const std::string& name, const std::string& value, bool isQuoted)
        {
            std::string piece = isQuoted
                ? nx::utils::buildString(name, "=\"", value, "\"")
                : nx::utils::buildString(name, '=', value);
            nx::utils::buildString(dest, isFirst ? "" : ", ", std::move(piece));
            isFirst = false;
        };

    auto params = this->params;
    for (const char* name: predefinedOrder)
    {
        auto itr = params.find(name);
        if (itr != params.end())
        {
            serializeParam(itr->first, itr->second, needQuoteForParam(itr->first));
            params.erase(itr);
        }
    }
    for (auto itr = params.begin(); itr != params.end(); ++itr)
        serializeParam(itr->first, itr->second, needQuoteForParam(itr->first));
}

bool BearerCredentials::parse(const std::string_view& str)
{
    token = str;
    return !token.empty();
}

void BearerCredentials::serialize(nx::Buffer* const dest) const
{
    dest->append(token);
}

//-------------------------------------------------------------------------------------------------
// Authorization.

Authorization::Authorization():
    authScheme(AuthScheme::none)
{
    basic = nullptr;
}

Authorization::Authorization(const AuthScheme::Value& authSchemeVal):
    authScheme(authSchemeVal)
{
    switch (authScheme)
    {
        case AuthScheme::none:
            basic = nullptr;
            return;

        case AuthScheme::basic:
            basic = new BasicCredentials();
            return;

        case AuthScheme::digest:
            digest = new DigestCredentials();
            return;

        case AuthScheme::bearer:
            bearer = new BearerCredentials();
            return;
    }

    basic = nullptr;
    NX_ASSERT(false, "Unexpected value: %1", static_cast<int>(authSchemeVal));
}

Authorization::Authorization(Authorization&& right):
    authScheme(right.authScheme),
    basic(right.basic)
{
    right.authScheme = AuthScheme::none;
    right.basic = nullptr;
}

Authorization::Authorization(const Authorization& right):
    authScheme(right.authScheme)
{
    switch (authScheme)
    {
        case AuthScheme::none:
            return;
        case AuthScheme::basic:
            basic = new BasicCredentials(*right.basic);
            return;
        case AuthScheme::digest:
            digest = new DigestCredentials(*right.digest);
            return;
        case AuthScheme::bearer:
            bearer = new BearerCredentials(*right.bearer);
            return;
    }

    NX_ASSERT(false, "Unexpected value: %1", static_cast<int>(authScheme));
}

Authorization::~Authorization()
{
    clear();
}

Authorization& Authorization::operator=(Authorization&& right)
{
    clear();

    authScheme = right.authScheme;
    basic = right.basic;

    right.authScheme = AuthScheme::none;
    right.basic = nullptr;

    return *this;
}

bool Authorization::parse(const std::string_view& str)
{
    clear();

    const auto authSchemeEndPos = str.find(" ");
    if (authSchemeEndPos == std::string_view::npos)
        return false;

    authScheme = AuthScheme::fromString(str.substr(0, authSchemeEndPos));
    const auto authParamsStr = str.substr(authSchemeEndPos + 1);
    switch (authScheme)
    {
        case AuthScheme::none:
            return false;

        case AuthScheme::basic:
            basic = new BasicCredentials();
            return basic->parse(authParamsStr);

        case AuthScheme::digest:
            digest = new DigestCredentials();
            return digest->parse(authParamsStr);

        case AuthScheme::bearer:
            bearer = new BearerCredentials();
            return bearer->parse(authParamsStr);

        default:
            return false;
    }

    return false;
}

void Authorization::serialize(nx::Buffer* dest) const
{
    nx::utils::buildString(dest, AuthScheme::toString(authScheme), ' ');

    if (authScheme == AuthScheme::basic)
        basic->serialize(dest);
    else if (authScheme == AuthScheme::digest)
        digest->serialize(dest);
    else if (authScheme == AuthScheme::bearer)
        bearer->serialize(dest);
    else
        NX_ASSERT(false, "Unexpected value: %1", static_cast<int>(authScheme));
}

nx::Buffer Authorization::serialized() const
{
    nx::Buffer dest;
    serialize(&dest);
    return dest;
}

std::string Authorization::toString() const
{
    return serialized().takeStdString();
}

void Authorization::clear()
{
    switch (authScheme)
    {
        case AuthScheme::none:
            break;

        case AuthScheme::basic:
            delete basic;
            break;

        case AuthScheme::digest:
            delete digest;
            break;

        case AuthScheme::bearer:
            delete bearer;
            break;

        default:
            NX_ASSERT(false, "Invalid value: %1", static_cast<int>(authScheme));
            break;
    }

    authScheme = AuthScheme::none;
    basic = nullptr;
}

std::string Authorization::userid() const
{
    switch (authScheme)
    {
        case AuthScheme::none:
            return std::string();

        case AuthScheme::basic:
            return basic->userid;

        case AuthScheme::digest:
            return digest->userid;

        case AuthScheme::bearer:
            return bearer->token;
    }

    NX_ASSERT(false, "Invalid value: %1", static_cast<int>(authScheme));
    return NX_FMT("InvalidScheme(%1)", static_cast<int>(authScheme)).toStdString();
}

BasicAuthorization::BasicAuthorization(
    const std::string& userName,
    const std::string& userPassword)
    :
    Authorization(AuthScheme::basic)
{
    basic->userid = userName;
    basic->password = userPassword;
}


DigestAuthorization::DigestAuthorization():
    Authorization(AuthScheme::digest)
{
}

DigestAuthorization::DigestAuthorization(DigestAuthorization&& right):
    Authorization(std::move(right))
{
}

DigestAuthorization::DigestAuthorization(const DigestAuthorization& right):
    Authorization(right)
{
}

void DigestAuthorization::addParam(const std::string& name, const std::string& value)
{
    if (name == "username")
        digest->userid = value;

    digest->params.emplace(name, value);
}

BearerAuthorization::BearerAuthorization(const std::string& token):
    Authorization(AuthScheme::bearer)
{
    bearer->token = token;
}

//-------------------------------------------------------------------------------------------------
// WWWAuthenticate.

WWWAuthenticate::WWWAuthenticate(AuthScheme::Value authScheme):
    authScheme(authScheme)
{
}

std::string WWWAuthenticate::getParam(const std::string& key) const
{
    const auto it = params.find(key);
    return it != params.end() ? it->second : std::string();
}

bool WWWAuthenticate::parse(const std::string_view& str)
{
    const auto authSchemeEndPos = str.find(' ');
    authScheme = AuthScheme::fromString(str.substr(0, authSchemeEndPos));
    if (authScheme == AuthScheme::none)
        return false;

    if (authSchemeEndPos != std::string_view::npos && str.find('=') != std::string_view::npos)
        nx::utils::splitNameValuePairs(
            str.substr(authSchemeEndPos + 1), ',', '=',
            std::inserter(params, params.end()));

    return true;
}

void WWWAuthenticate::serialize(nx::Buffer* dest) const
{
    nx::utils::buildString(dest, AuthScheme::toString(authScheme), ' ');

    nx::utils::join(
        dest,
        params.begin(), params.end(), ", ",
        [](auto* out, const auto& param)
        {
            nx::utils::buildString(out, param.first, "=\"", param.second, '"');
        });
}

nx::Buffer WWWAuthenticate::serialized() const
{
    nx::Buffer dest;
    serialize(&dest);
    return dest;
}

std::string WWWAuthenticate::toString() const
{
    auto res = serialized();
    return res.takeStdString();
}

//-------------------------------------------------------------------------------------------------
// Accept-Encoding.

AcceptEncodingHeader::AcceptEncodingHeader(const std::string_view& str)
{
    parse(str);
}

void AcceptEncodingHeader::parse(const std::string_view& str)
{
    m_anyCodingQValue.reset();

    nx::utils::split(
        str, ',',
        [this](const std::string_view& contentCodingStr)
        {
            const auto [tokens, count] = nx::utils::split_n<2>(contentCodingStr, ';');
            if (count == 0)
                return;

            const auto contentCoding = nx::utils::trim(tokens.front());

            double qValue = 1.0;
            if (count > 1)
            {
                const auto qValueStr = nx::utils::trim(tokens[1]);
                if (nx::utils::startsWith(qValueStr, "q="))
                    qValue = std::stod(std::string(qValueStr.substr(2)));
            }

            if (contentCoding == ANY_CODING)
                m_anyCodingQValue = qValue;
            else
                m_codings[std::string(contentCoding)] = qValue;
        });
}

bool AcceptEncodingHeader::encodingIsAllowed(
    const std::string& encodingName,
    double* q) const
{
    auto codingIter = m_codings.find(encodingName);
    if (codingIter == m_codings.end())
    {
        //encoding is not explicitly specified
        if (m_anyCodingQValue)
        {
            if (q)
                *q = *m_anyCodingQValue;
            return *m_anyCodingQValue > 0.0;
        }

        return encodingName == IDENTITY_CODING;
    }

    if (q)
        *q = codingIter->second;
    return codingIter->second > 0.0;
}

const std::map<std::string, double>& AcceptEncodingHeader::allEncodings() const
{
    return m_codings;
}

//-------------------------------------------------------------------------------------------------
// Range.

Range::Range() = default;

bool Range::parse(const std::string_view& str)
{
    nx::utils::split(
        str, ',',
        [this](const std::string_view& simpleRangeStr)
        {
            RangeSpec rangeSpec;
            const auto [tokens, count] = nx::utils::split_n<2>(
                simpleRangeStr, '-', nx::utils::GroupToken::none, nx::utils::SplitterFlag::noFlags);
            if (count == 0)
                return;

            rangeSpec.start = nx::utils::stoull(tokens.front());
            if (count == 1)
                rangeSpec.end = rangeSpec.start;
            else if (!tokens[1].empty())
                rangeSpec.end = nx::utils::stoull(tokens[1]);

            if (rangeSpec.end && rangeSpec.end < rangeSpec.start)
                return;

            rangeSpecList.push_back(std::move(rangeSpec));
        });

    return true;
}

bool Range::validateByContentSize(size_t contentSize) const
{
    for (const RangeSpec& rangeSpec: rangeSpecList)
    {
        if ((rangeSpec.start >= contentSize) || (rangeSpec.end && *rangeSpec.end >= contentSize))
            return false;
    }

    return true;
}

bool Range::empty() const
{
    return rangeSpecList.empty();
}

bool Range::full(size_t contentSize) const
{
    if (contentSize == 0)
        return true;

    //map<start, end>
    std::map<quint64, quint64> rangesSorted;
    for (const RangeSpec& rangeSpec : rangeSpecList)
    {
        rangesSorted.emplace(
            rangeSpec.start,
            rangeSpec.end ? *rangeSpec.end : contentSize);
    }

    quint64 curPos = 0;
    for (const auto& range: rangesSorted)
    {
        if (range.first > curPos)
            return false;
        if (range.second >= curPos)
            curPos = range.second + 1;
    }

    return curPos >= contentSize;
}

quint64 Range::totalRangeLength(size_t contentSize) const
{
    if (contentSize == 0 || rangeSpecList.empty())
        return 0;

    //map<start, end>
    std::map<quint64, quint64> rangesSorted;
    for (const RangeSpec& rangeSpec : rangeSpecList)
    {
        rangesSorted.emplace(
            rangeSpec.start,
            rangeSpec.end ? *rangeSpec.end : contentSize);
    }

    quint64 curPos = 0;
    quint64 totalLength = 0;
    for (const auto& range : rangesSorted)
    {
        if (curPos < range.first)
            curPos = range.first;
        if (range.second < curPos)
            continue;
        const quint64 endPos = std::min<quint64>(contentSize - 1, range.second);
        totalLength += endPos - curPos + 1;
        curPos = endPos + 1;
        if (curPos >= contentSize)
            break;
    }

    return totalLength;
}


//-------------------------------------------------------------------------------------------------
// Content-Range.

ContentRange::ContentRange():
    unitName("bytes")
{
}

quint64 ContentRange::rangeLength() const
{
    NX_ASSERT(!rangeSpec.end || (rangeSpec.end >= rangeSpec.start));

    if (rangeSpec.end)
        return *rangeSpec.end - rangeSpec.start + 1;   //both boundaries are inclusive
    else if (instanceLength)
        return *instanceLength - rangeSpec.start;
    else
        return 1;   //since both boundaries are inclusive, 0-0 means 1 byte (the first one)
}

std::string ContentRange::toString() const
{
    NX_ASSERT(!rangeSpec.end || (rangeSpec.end >= rangeSpec.start));

    quint64 rangeEnd = 0;
    if (rangeSpec.end)
        rangeEnd = *rangeSpec.end;
    else if (instanceLength)
        rangeEnd = *instanceLength - 1;
    else
        rangeEnd = rangeSpec.start;

    std::string suffix = instanceLength
        ? nx::utils::buildString('/', *instanceLength)
        : "/*";

    return nx::utils::buildString(
        unitName, ' ',
        rangeSpec.start, '-', rangeEnd,
        suffix);
}


//-------------------------------------------------------------------------------------------------
// Via.

bool Via::parse(const std::string_view& strValue)
{
    if (strValue.empty())
        return true;

    // TODO: split

    //introducing loop counter to guarantee method finiteness in case of bug in code
    for (size_t curEntryEnd = strValue.find_first_of(','), curEntryStart = 0, i = 0;
        curEntryStart != std::string_view::npos && (i < 1000);
        curEntryStart = (curEntryEnd == std::string_view::npos ? curEntryEnd : curEntryEnd + 1),
            curEntryEnd = strValue.find_first_of(',', curEntryEnd + 1),
            ++i)
    {
        ProxyEntry entry;

        //skipping spaces at the start of entry
        while ((curEntryStart < (curEntryEnd == std::string_view::npos ? strValue.size() : curEntryEnd)) &&
            (strValue.at(curEntryStart) == ' '))
        {
            ++curEntryStart;
        }

        //curEntryStart points first char after comma
        size_t receivedProtoEnd = strValue.find_first_of(' ', curEntryStart);
        if (receivedProtoEnd == std::string_view::npos)
            return false;
        const auto protoNameVersion = strValue.substr(curEntryStart, receivedProtoEnd - curEntryStart);
        size_t nameVersionSep = protoNameVersion.find_first_of('/');
        if (nameVersionSep == std::string_view::npos)
        {
            //only version present
            entry.protoVersion = protoNameVersion;
        }
        else
        {
            entry.protoName = std::string(protoNameVersion.substr(0, nameVersionSep));
            entry.protoVersion = std::string(protoNameVersion.substr(nameVersionSep + 1));
        }

        size_t receivedByStart = strValue.find_first_not_of(' ', receivedProtoEnd + 1);
        if (receivedByStart == std::string_view::npos || receivedByStart > curEntryEnd)
            return false;   //no receivedBy field

        size_t receivedByEnd = strValue.find_first_of(' ', receivedByStart);
        if (receivedByEnd == std::string_view::npos || (receivedByEnd > curEntryEnd))
        {
            receivedByEnd = curEntryEnd;
        }
        else
        {
            //comment present
            size_t commentStart = strValue.find_first_not_of(' ', receivedByEnd + 1);
            if (commentStart != std::string_view::npos && commentStart < curEntryEnd)
                entry.comment = strValue.substr(commentStart, curEntryEnd == std::string_view::npos ? -1 : (curEntryEnd - commentStart)); //space are allowed in comment
        }
        entry.receivedBy = strValue.substr(receivedByStart, receivedByEnd - receivedByStart);

        entries.push_back(std::move(entry));
    }

    return true;
}

std::string Via::toString() const
{
    return nx::utils::join(
        entries.begin(), entries.end(), ", ",
        [](auto* out, const auto& entry)
        {
            nx::utils::buildString(out,
                entry.protoName, entry.protoName ? "/" : "", entry.protoVersion,
                ' ', entry.receivedBy,
                !entry.comment.empty() ? " " : "", entry.comment);
        });
}

//-------------------------------------------------------------------------------------------------
// Keep-Alive.

KeepAlive::KeepAlive(
    std::chrono::seconds _timeout,
    std::optional<int> _max)
    :
    timeout(std::move(_timeout)),
    max(std::move(_max))
{
}

bool KeepAlive::parse(const std::string_view& str)
{
    max.reset();

    bool timeoutFound = false;
    nx::utils::split(
        str, ',',
        [this, &timeoutFound](std::string_view param)
        {
            param = nx::utils::trim(param);
            const auto [tokens, count] = nx::utils::split_n<2>(param, '=');
            if (count < 2)
                return;

            if (tokens[0] == "timeout")
            {
                timeout = std::chrono::seconds(nx::utils::stoul(tokens[1]));
                timeoutFound = true;
            }
            else if (tokens[0] == "max")
            {
                max = nx::utils::stoul(tokens[1]);
            }
        });

    return timeoutFound;
}

std::string KeepAlive::toString() const
{
    return nx::utils::buildString(
        "timeout=", timeout.count(),
        max ? ", max=" : "",
        max);
}

//-------------------------------------------------------------------------------------------------
// Server

// TODO: #akolesnikov Better replace following macro with constants.

#if defined(_WIN32)
#   define COMPATIBILITY_SERVER_STRING "Apache/2.4.16 (MSWin)"
#   if defined(_WIN64)
#       define COMMON_USER_AGENT "Mozilla/5.0 (Windows NT 6.1; WOW64)"
#   else
#       define COMMON_USER_AGENT "Mozilla/5.0 (Windows NT 6.1; Win32)"
#   endif
#elif defined(__linux__)
#   define COMPATIBILITY_SERVER_STRING "Apache/2.4.16 (Unix)"
#   ifdef __LP64__
#       define COMMON_USER_AGENT "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:36.0)"
#   else
#       define COMMON_USER_AGENT "Mozilla/5.0 (X11; Ubuntu; Linux x86; rv:36.0)"
#   endif
#elif defined(__APPLE__)
#   define COMPATIBILITY_SERVER_STRING "Apache/2.4.16 (Unix)"
#   define COMMON_USER_AGENT "Mozilla/5.0 (Macosx x86_64)"
#else   //some other unix (e.g., FreeBSD)
#   define COMPATIBILITY_SERVER_STRING "Apache/2.4.16 (Unix)"
#   define COMMON_USER_AGENT "Mozilla/5.0"
#endif

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
#   define PRODUCT_NAME_SUFFIX " Mobile"
#else
#   define PRODUCT_NAME_SUFFIX ""
#endif

//-------------------------------------------------------------------------------------------------
// Server::Product

std::string Server::Product::toString() const
{
    return nx::utils::buildString(
        name,
        !version.empty() ? "/" : "",
        version,
        !comment.empty() ? " (" : "",
        comment,
        !comment.empty() ? ")" : "");
}

bool Server::Product::operator==(const Server::Product& right) const
{
    return name == right.name
        && version == right.version
        && comment == right.comment;
}

Server::Product Server::Product::fromString(const std::string_view& str)
{
    Product product;

    const auto [tokens, count] = nx::utils::split_n<2>(
        str, ' ', nx::utils::GroupToken::roundBrackets, nx::utils::skipEmpty);
    if (count >= 1)
    {
        // token ["/" product-version]
        const auto [productTokens, count] = nx::utils::split_n<2>(tokens[0], '/', 0, 0);
        if (count >= 1)
            product.name = std::string(productTokens[0]);
        if (count >= 2)
            product.version = std::string(productTokens[1]);
    }

    if (count >= 2)
        product.comment = std::string(nx::utils::trim(tokens[1], "()"));

    return product;
}

//-------------------------------------------------------------------------------------------------
// Server

Server::Server()
{
    products.push_back(Product{
        nx::branding::brand().toStdString(),
        nx::build_info::vmsVersion().toStdString(),
        nx::branding::company().toStdString()});
}

bool Server::operator==(const Server& right) const
{
    return products == right.products;
}

bool Server::parse(const std::string_view& serverString)
{
    // "Nx/1.0 Mozilla/5.0 (Windows NT 6.1; WOW64)"

    products.clear();

    bool ok = true;
    nx::utils::split(
        serverString, ' ',
        [this, &ok](const std::string_view& token)
        {
            if (token.starts_with('(') && token.ends_with(')')) // comment?
            {
                if (!products.empty())
                    products.back().comment = std::string(nx::utils::trim(token, "()"));
            }
            else
            {
                products.push_back(Product::fromString(token));
                // empty product name is an error.
                ok = ok && !products.back().name.empty();
            }
        },
        nx::utils::GroupToken::roundBrackets, nx::utils::skipEmpty);

    return !products.empty() && ok;
}

std::string Server::toString() const
{
    std::string result;

    for (const auto& product: products)
        nx::utils::buildString(&result, product.toString(), ' ');

    result += COMPATIBILITY_SERVER_STRING;
    return result;
}

//-------------------------------------------------------------------------------------------------

bool StrictTransportSecurity::operator==(const StrictTransportSecurity& rhs) const
{
    return maxAge == rhs.maxAge
        && includeSubDomains == rhs.includeSubDomains
        && preload == rhs.preload;
}

bool StrictTransportSecurity::parse(const std::string_view& str)
{
    includeSubDomains = false;
    preload = false;

    nx::utils::splitNameValuePairs(
        str, ';', '=',
        [this](const std::string_view& name, const std::string_view& value)
        {
            if (name == "max-age")
                maxAge = std::chrono::seconds(nx::utils::stoi(value));
            else if (name == "includeSubDomains")
                includeSubDomains = true;
            else if (name == "preload")
                preload = true;
        });

    return true;
}

std::string StrictTransportSecurity::toString() const
{
    return nx::utils::buildString(
        "max-age=", maxAge.count(),
        includeSubDomains ? ";includeSubDomains" : "",
        preload ? ";preload" : "");
}

//-------------------------------------------------------------------------------------------------

bool XForwardedFor::parse(const std::string_view& str)
{
    std::size_t count = 0;
    nx::utils::split(
        str, ',',
        [this, &count](const std::string_view& token)
        {

            const auto trimmedToken = nx::utils::trim(token);
            if (count == 0)
                client = trimmedToken;
            else
                proxies.push_back(std::string(trimmedToken));
            ++count;
        });

    return count > 0;
}

std::string XForwardedFor::toString() const
{
    return nx::utils::buildString(
        client,
        client.empty() ? "" : ", ",
        nx::utils::join(proxies.begin(), proxies.end(), ", "));
}

//-------------------------------------------------------------------------------------------------

bool ForwardedElement::parse(const std::string_view& str)
{
    int count = 0;
    nx::utils::splitNameValuePairs(
        str, ';', '=',
        [this, &count](const std::string_view& name, const std::string_view& value)
        {
            ++count;

            if (nx::utils::stricmp(name, "by") == 0)
                by = value;
            else if (nx::utils::stricmp(name, "for") == 0)
                for_ = value;
            else if (nx::utils::stricmp(name, "host") == 0)
                host = value;
            else if (nx::utils::stricmp(name, "proto") == 0)
                proto = value;
        });

    return count > 0;
}

std::string ForwardedElement::toString() const
{
#if 0
    // TODO: #akolesnikov Uncomment and support the following implementation.
    // It does the thing with a single allocation (with quoteIfNeeded returning StringViewArray).
    // Qt-based implementation had ~30 allocations, the current implementation does 5-9 allocations.
    using namespace nx::utils;

    std::array<nx::utils::StringViewArray<4>, 4> tokens;
    std::size_t tokenCount = 0;

    if (!by.empty())
        tokens[tokenCount++] = concatArrays(makeStringViewArray<1>("by="), quoteIfNeeded(by));
    if (!for_.empty())
        tokens[tokenCount++] = concatArrays(makeStringViewArray<1>("for="), quoteIfNeeded(for_));
    if (!host.empty())
        tokens[tokenCount++] = concatArrays(makeStringViewArray<1>("host="), quoteIfNeeded(host));
    if (!proto.empty())
        tokens[tokenCount++] = concatArrays(makeStringViewArray<1>("proto="), quoteIfNeeded(proto));

    return nx::utils::join(
        tokens.begin(), tokens.begin() + tokenCount,
        ';');
#else
    using namespace nx::utils;

    std::array<std::string, 4> tokens;
    std::size_t tokenCount = 0;

    if (!by.empty())
        tokens[tokenCount++] = nx::utils::buildString("by=", quoteIfNeeded(by));
    if (!for_.empty())
        tokens[tokenCount++] = nx::utils::buildString("for=", quoteIfNeeded(for_));
    if (!host.empty())
        tokens[tokenCount++] = nx::utils::buildString("host=", quoteIfNeeded(host));
    if (!proto.empty())
        tokens[tokenCount++] = nx::utils::buildString("proto=", quoteIfNeeded(proto));

    return nx::utils::join(
        tokens.begin(), tokens.begin() + tokenCount,
        ';');
#endif
}

bool ForwardedElement::operator==(const ForwardedElement& right) const
{
    return by == right.by
        && for_ == right.for_
        && host == right.host
        && proto == right.proto;
}

std::string ForwardedElement::quoteIfNeeded(const std::string_view& str) const
{
    if (str.find(':') != std::string::npos)
        return nx::utils::buildString('\"', str, '\"');
    else
        return std::string(str);
}

Forwarded::Forwarded(std::vector<ForwardedElement> elements):
    elements(std::move(elements))
{
}

bool Forwarded::parse(const std::string_view& str)
{
    nx::utils::split(
        str, ',',
        [this](const std::string_view& token)
        {
            const auto trimmedToken = nx::utils::trim(token);
            if (trimmedToken.empty())
                return;

            ForwardedElement element;
            if (element.parse(trimmedToken))
                elements.push_back(std::move(element));
        });

    return !elements.empty();
}

std::string Forwarded::toString() const
{
    return nx::utils::join(elements.begin(), elements.end(), ", ");
}

bool Forwarded::operator==(const Forwarded& right) const
{
    return elements == right.elements;
}

//-------------------------------------------------------------------------------------------------

// TODO: Apply default charset (UTF-8) to every text based content types when it's not specified.
// Currently UTF-8 is explicitly specified for texts only because some browsers may fail otherwise.
const ContentType ContentType::kPlain("text/plain; charset=utf-8");
const ContentType ContentType::kHtml("text/html; charset=utf-8");
const ContentType ContentType::kXml("application/xml");
const ContentType ContentType::kForm("application/x-www-form-urlencoded");
const ContentType ContentType::kJson("application/json");
const ContentType ContentType::kUbjson("application/ubjson");
const ContentType ContentType::kBinary("application/octet-stream");

ContentType::ContentType(const std::string_view& str)
{
    const auto [records, recordCount] = nx::utils::split_n<2>(str, ';');
    if (recordCount == 0)
        return;

    value = records[0];
    nx::utils::toLower(&value);

    if (recordCount == 1)
        return;

    const auto charsetExpression = nx::utils::trim(records[1]);
    const auto [charsetTokens, count] = nx::utils::split_n<2>(charsetExpression, '=');
    if (count != 2)
        return;

    if (charsetTokens[0] == "charset")
    {
        charset = charsetTokens[1];
        nx::utils::toLower(&charset);
    }
}

ContentType::ContentType(const char* str):
    ContentType(std::string_view(str))
{
}

std::string ContentType::toString() const
{
    return nx::utils::buildString(
        value, !charset.empty() ? "; charset=" : "", charset);
}

bool ContentType::operator==(const ContentType& rhs) const
{
    if (value != rhs.value)
        return false;

    if (charset.empty() || rhs.charset.empty())
        return true;

    return charset == rhs.charset;
}

bool ContentType::operator==(const std::string& rhs) const
{
    return value == rhs;
}

//-------------------------------------------------------------------------------------------------

Host::Host(const SocketAddress& endpoint):
    m_endpoint(endpoint)
{
}

std::string Host::toString() const
{
    if (m_endpoint.port == DEFAULT_HTTP_PORT || m_endpoint.port == DEFAULT_HTTPS_PORT)
        return m_endpoint.address.toString();
    else
        return m_endpoint.toString();
}

} // namespace header

//-------------------------------------------------------------------------------------------------

void ChunkHeader::clear()
{
    chunkSize = 0;
    extensions.clear();
}

int ChunkHeader::parse(const ConstBufferRefType& buf)
{
    // TODO: Add more tests and then refactor this function using string utilities.

    const char* curPos = buf.data();
    const char* dataEnd = curPos + buf.size();

    enum State
    {
        readingChunkSize,
        readingExtName,
        readingExtVal,
        readingCRLF
    }
    state = readingChunkSize;
    chunkSize = 0;
    const char* extNameStart = 0;
    const char* extSepPos = 0;
    const char* extValStart = 0;

    for (; curPos < dataEnd; ++curPos)
    {
        const char ch = *curPos;
        switch (ch)
        {
            case ';':
                if (state == readingExtName)
                {
                    if (curPos == extNameStart)
                        return -1;  //empty extension
                    extensions.push_back(std::make_pair(
                        std::string(extNameStart, curPos - extNameStart),
                        std::string()));
                }
                else if (state == readingExtVal)
                {
                    if (extSepPos == extNameStart)
                        return -1;  //empty extension

                    if (*extValStart == '"')
                        ++extValStart;
                    int extValSize = curPos - extValStart;
                    if (extValSize > 0 && *(extValStart + extValSize - 1) == '"')
                        --extValSize;

                    extensions.push_back(std::make_pair(
                        std::string(extNameStart, extSepPos - extNameStart),
                        std::string(extValStart, extValSize)));
                }

                state = readingExtName;
                extNameStart = curPos + 1;
                continue;

            case '=':
                if (state == readingExtName)
                {
                    state = readingExtVal;
                    extSepPos = curPos;
                    extValStart = curPos + 1;
                }
                else
                {
                    //unexpected token
                    return -1;
                }
                break;

            case '\r':
                if (state == readingExtName)
                {
                    if (curPos == extNameStart)
                        return -1;  //empty extension
                    extensions.push_back(std::make_pair(
                        std::string(extNameStart, curPos - extNameStart),
                        std::string()));
                }
                else if (state == readingExtVal)
                {
                    if (extSepPos == extNameStart)
                        return -1;  //empty extension

                    if (*extValStart == '"')
                        ++extValStart;
                    int extValSize = curPos - extValStart;
                    if (extValSize > 0 && *(extValStart + extValSize - 1) == '"')
                        --extValSize;

                    extensions.push_back(std::make_pair(
                        std::string(extNameStart, extSepPos - extNameStart),
                        std::string(extValStart, extValSize)));
                }

                if ((dataEnd - curPos < 2) || *(curPos + 1) != '\n')
                    return -1;
                return curPos + 2 - buf.data();

            default:
                if (state == readingChunkSize)
                {
                    if (ch >= '0' && ch <= '9')
                        chunkSize = (chunkSize << 4) | (ch - '0');
                    else if (ch >= 'a' && ch <= 'f')
                        chunkSize = (chunkSize << 4) | (ch - 'a' + 10);
                    else if (ch >= 'A' && ch <= 'F')
                        chunkSize = (chunkSize << 4) | (ch - 'A' + 10);
                    else
                        return -1;  //unexpected token
                }
                break;
        }
    }

    return -1;
}

int ChunkHeader::serialize(nx::Buffer* /*dest*/) const
{
    // TODO #akolesnikov
    return -1;
}

std::string userAgentString()
{
    static const auto defaultUserAgentString = nx::utils::buildString(
        nx::branding::vmsName().toStdString(),
        PRODUCT_NAME_SUFFIX,
        '/',
        nx::build_info::vmsVersion().toStdString(),
        " (", nx::branding::company().toStdString(), ") ",
        COMMON_USER_AGENT);

    return defaultUserAgentString;
}

header::UserAgent defaultUserAgent()
{
    // NOTE: By default, userAgent.products contains the VMS product name/version.
    header::UserAgent userAgent;

    // Can't just do header::UserAgent::parse(userAgentString()) since some VMS names
    // contain spaces which violate User-Agent format making it impossible to parse properly.

    userAgent.products.push_back(
        header::UserAgent::Product::fromString(COMMON_USER_AGENT));

    return userAgent;
}

std::string serverString()
{
    static const auto defaultServerString = nx::utils::buildString(
        nx::branding::vmsName().toStdString(),
        '/',
        nx::build_info::vmsVersion().toStdString(),
        " (", nx::branding::company().toStdString(), ") ",
        COMPATIBILITY_SERVER_STRING);

    return defaultServerString;
}

std::string formatDateTime(const QDateTime& value)
{
    // Starts with Monday to correspond to QDate::dayOfWeek() return value: 1 for Monday, 7 for Sunday
    static constexpr const char* kWeekDays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    static constexpr const char* kMonths[] = {
         "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    if (value.isNull() || !value.isValid())
        return std::string();

    QDateTime utc = value.toUTC();
    const QDate& date = utc.date();
    const QTime& time = utc.time();

    char strDateBuf[256];
    snprintf(strDateBuf, sizeof(strDateBuf), "%s, %02d %s %d %02d:%02d:%02d GMT",
        kWeekDays[date.dayOfWeek() - 1],
        date.day(),
        kMonths[date.month() - 1],
        date.year(),
        time.hour(),
        time.minute(),
        time.second());

    return std::string(strDateBuf);
}

QDateTime parseDate(const std::string_view& str)
{
    auto date = nx::utils::trim(str);

    // Sanity check. parseAnsiCDate() has hard-coded array access.
    if (date.length() < 8)
        return QDateTime();

    // If the string ends with " GMT" it is either RFC 1123 or RFC 850
    if (nx::utils::endsWith(date, " GMT"))
    {
        // QDateTime::fromString() fails if string contains " GMT"
        // because "M" is used as a formatter.
        date = date.substr(0, date.length() - 4);
        QDateTime parsedDate;

        // RFC 1123 date: first three characters are abbreviated day of the week, like "Sun",
        // followed by a comma
        if (date[3] == ',')
            parsedDate = parseRfc1123Date(date);

        if (!parsedDate.isValid())
            parsedDate = parseRfc850Date(date);

        if (parsedDate.isValid())
        {
            parsedDate.setTimeSpec(Qt::UTC);
            return parsedDate;
        }
    }

    // Fall back to ANSI C date.
    return parseAnsiCDate(date);
}

} // namespace nx::network::http
