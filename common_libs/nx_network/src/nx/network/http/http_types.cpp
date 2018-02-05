#include "http_types.h"

#include <stdio.h>
#include <cstring>
#include <memory>

#include <QtCore/QString>

#include <nx/utils/app_info.h>
#include <nx/utils/string.h>

#ifdef _WIN32
static int strcasecmp(const char * str1, const char * str2) { return strcmpi(str1, str2); }
static int strncasecmp(const char * str1, const char * str2, size_t n) { return strnicmp(str1, str2, n); }
#endif

namespace nx {
namespace network {
namespace http {

const char* const kUrlSchemeName = "http";
const char* const kSecureUrlSchemeName = "https";

int strcasecmp(const StringType& one, const StringType& two)
{
    if (one.size() < two.size())
        return -1;
    if (one.size() > two.size())
        return 1;
#ifdef _WIN32
    return _strnicmp(one.constData(), two.constData(), one.size());
#else
    return strncasecmp(one.constData(), two.constData(), one.size());
#endif
}

int defaultPortForScheme(const StringType& scheme)
{
    if (strcasecmp(scheme, StringType(kUrlSchemeName)) == 0)
        return DEFAULT_HTTP_PORT;
    if (strcasecmp(scheme, StringType(kSecureUrlSchemeName)) == 0)
        return DEFAULT_HTTPS_PORT;
    return -1;
}

StringType getHeaderValue(const HttpHeaders& headers, const StringType& headerName)
{
    HttpHeaders::const_iterator it = headers.find(headerName);
    return it == headers.end() ? StringType() : it->second;
}

bool readHeader(
    const HttpHeaders& headers,
    const StringType& headerName,
    int* value)
{
    auto it = headers.find(headerName);
    if (it == headers.end())
        return false;
    *value = it->second.toInt();
    return true;
}

HttpHeaders::iterator insertOrReplaceHeader(HttpHeaders* const headers, const HttpHeader& newHeader)
{
    HttpHeaders::iterator existingHeaderIter = headers->lower_bound(newHeader.first);
    if ((existingHeaderIter != headers->end()) &&
        (strcasecmp(existingHeaderIter->first, newHeader.first) == 0))
    {
        existingHeaderIter->second = newHeader.second;  //replacing header
        return existingHeaderIter;
    }
    else
    {
        return headers->insert(existingHeaderIter, newHeader);
    }
}

HttpHeaders::iterator insertHeader(HttpHeaders* const headers, const HttpHeader& newHeader)
{
    HttpHeaders::iterator itr = headers->lower_bound(newHeader.first);
    return headers->insert(itr, newHeader);
}

void removeHeader(HttpHeaders* const headers, const StringType& headerName)
{
    HttpHeaders::iterator itr = headers->lower_bound(headerName);
    while (itr != headers->end() && itr->first == headerName)
        itr = headers->erase(itr);
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

void serializeHeaders(const HttpHeaders& headers, BufferType* const dstBuffer)
{
    for (HttpHeaders::const_iterator
        it = headers.begin();
        it != headers.end();
        ++it)
    {
        *dstBuffer += it->first;
        *dstBuffer += ": ";
        *dstBuffer += it->second;
        *dstBuffer += "\r\n";
    }
}

bool parseHeader(
    StringType* const headerName,
    StringType* const headerValue,
    const ConstBufferRefType& data)
{
    ConstBufferRefType headerNameRef;
    ConstBufferRefType headerValueRef;
    if (!parseHeader(&headerNameRef, &headerValueRef, data))
        return false;
    *headerName = headerNameRef;
    *headerValue = headerValueRef;
    return true;
}

bool parseHeader(
    ConstBufferRefType* const headerName,
    ConstBufferRefType* const headerValue,
    const ConstBufferRefType& data)
{
    //skipping whitespace at start
    const size_t headerNameStart = nx::utils::find_first_not_of(data, " ");
    if (headerNameStart == nx::utils::BufferNpos)
        return false;
    const size_t headerNameEnd = nx::utils::find_first_of(data, " :", headerNameStart);
    if (headerNameEnd == nx::utils::BufferNpos)
        return false;

    const size_t headerSepPos = data[headerNameEnd] == ':'
        ? headerNameEnd
        : nx::utils::find_first_of(data, ":", headerNameEnd);
    if (headerSepPos == nx::utils::BufferNpos)
        return false;
    const size_t headerValueStart = nx::utils::find_first_not_of(data, ": ", headerSepPos);
    if (headerValueStart == nx::utils::BufferNpos)
    {
        *headerName = data.mid(headerNameStart, headerNameEnd - headerNameStart);
        return true;
    }

    //skipping separators after headerValue
    const size_t headerValueEnd = nx::utils::find_last_not_of(data, " \n\r", headerValueStart);
    if (headerValueEnd == nx::utils::BufferNpos)
        return false;

    *headerName = data.mid(headerNameStart, headerNameEnd - headerNameStart);
    *headerValue = data.mid(headerValueStart, headerValueEnd + 1 - headerValueStart);
    return true;
}

HttpHeader parseHeader(const ConstBufferRefType& data)
{
    ConstBufferRefType headerNameRef;
    ConstBufferRefType headerValueRef;
    parseHeader(&headerNameRef, &headerValueRef, data);
    return HttpHeader(headerNameRef, headerValueRef);
}

//-------------------------------------------------------------------------------------------------
// StatusCode.

namespace StatusCode {

StringType toString(int val)
{
    return toString(Value(val));
}

StringType toString(Value val)
{
    switch (val)
    {
        case _continue:
            return StringType("Continue");
        case switchingProtocols:
            return StringType("Switching Protocols");
        case ok:
            return StringType("OK");
        case noContent:
            return StringType("No Content");
        case partialContent:
            return StringType("Partial Content");
        case multipleChoices:
            return StringType("Multiple Choices");
        case movedPermanently:
            return StringType("Moved Permanently");
        case found:
            return StringType("Found");
        case seeOther:
            return StringType("See Other");
        case notModified:
            return StringType("Not Modified");
        case badRequest:
            return StringType("Bad Request");
        case unauthorized:
            return StringType("Unauthorized");
        case forbidden:
            return StringType("Forbidden");
        case notFound:
            return StringType("Not Found");
        case notAllowed:
            return StringType("Not Allowed");
        case notAcceptable:
            return StringType("Not Acceptable");
        case proxyAuthenticationRequired:
            return StringType("Proxy Authentication Required");
        case rangeNotSatisfiable:
            return StringType("Requested range not satisfiable");
        case internalServerError:
            return StringType("Internal Server Error");
        case notImplemented:
            return StringType("Not Implemented");
        case serviceUnavailable:
            return StringType("Service Unavailable");
        default:
            return StringType("Unknown_") + StringType::number(val);
    }
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
        case found:
            return false;

        default:
            // Message body is forbidden for informational status codes.
            if (statusCode / 100 == 1)
                return false;
            return true;
    }
}

} // namespace StatusCode

const StringType Method::get("GET");
const StringType Method::head("HEAD");
const StringType Method::post("POST");
const StringType Method::put("PUT");
const StringType Method::delete_("DELETE");
const StringType Method::options("OPTIONS");

bool Method::isMessageBodyAllowed(ValueType method)
{
    return method != get && method != head && method != delete_;
}

//-------------------------------------------------------------------------------------------------
// class MimeProtoVersion.

static size_t estimateSerializedDataSize(const MimeProtoVersion& val)
{
    return val.protocol.size() + 1 + val.version.size();
}

bool MimeProtoVersion::parse(const ConstBufferRefType& data)
{
    protocol.clear();
    version.clear();

    const int sepPos = data.indexOf('/');
    if (sepPos == -1)
        return false;
    protocol.append(data.constData(), sepPos);
    version.append(data.constData() + sepPos + 1, data.size() - (sepPos + 1));
    return true;
}

void MimeProtoVersion::serialize(BufferType* const dstBuffer) const
{
    dstBuffer->append(protocol);
    dstBuffer->append("/");
    dstBuffer->append(version);
}


//-------------------------------------------------------------------------------------------------
// class RequestLine.

static size_t estimateSerializedDataSize(const RequestLine& rl)
{
    return rl.method.size() + 1 + rl.url.toString().size() + 1 + estimateSerializedDataSize(rl.version) + 2;
}

bool RequestLine::parse(const ConstBufferRefType& data)
{
    enum ParsingState
    {
        psMethod,
        psUrl,
        psVersion,
        psDone
    }
    parsingState = psMethod;

    const char* str = data.constData();
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
                        method.append(tokenStart, str - tokenStart);
                        parsingState = psUrl;
                        break;
                    case psUrl:
                        url.setUrl(QLatin1String(tokenStart, str - tokenStart));
                        parsingState = psVersion;
                        break;
                    case psVersion:
                        version.parse(data.mid(tokenStart - data.constData(), str - tokenStart));
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

void RequestLine::serialize(BufferType* const dstBuffer) const
{
    *dstBuffer += method;
    *dstBuffer += " ";
    QByteArray path = url.toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters).toLatin1();
    *dstBuffer += path.isEmpty() ? "/" : path;
    *dstBuffer += " ";
    version.serialize(dstBuffer);
    *dstBuffer += "\r\n";
}

StringType RequestLine::toString() const
{
    BufferType buf;
    serialize(&buf);
    return buf;
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
    const size_t versionStart = 0;
    const size_t versionEnd = nx::utils::find_first_of(data, " ", versionStart);
    if (versionEnd == nx::utils::BufferNpos)
        return false;
    if (!version.parse(data.mid(versionStart, versionEnd - versionStart)))
        return false;

    const size_t statusCodeStart = nx::utils::find_first_not_of(data, " ", versionEnd);
    if (statusCodeStart == nx::utils::BufferNpos)
        return false;
    const size_t statusCodeEnd = nx::utils::find_first_of(data, " ", statusCodeStart);
    if (statusCodeEnd == nx::utils::BufferNpos)
        return false;
    statusCode = ((BufferType)data.mid(
        statusCodeStart,
        statusCodeEnd - statusCodeStart)).toUInt();

    const size_t reasonPhraseStart = nx::utils::find_first_not_of(data, " ", statusCodeEnd);
    if (reasonPhraseStart == nx::utils::BufferNpos)
        return false;
    const size_t reasonPhraseEnd = nx::utils::find_first_of(data, "\r\n", reasonPhraseStart);
    reasonPhrase = data.mid(
        reasonPhraseStart,
        reasonPhraseEnd == nx::utils::BufferNpos
            ? nx::utils::BufferNpos
            : reasonPhraseEnd - reasonPhraseStart);

    return true;
}

void StatusLine::serialize(BufferType* const dstBuffer) const
{
    version.serialize(dstBuffer);
    *dstBuffer += " ";
    char buf[11];
#ifdef _WIN32
    _snprintf(buf, sizeof(buf), "%d", statusCode);
#else
    snprintf(buf, sizeof(buf), "%d", statusCode);
#endif
    *dstBuffer += buf;
    *dstBuffer += " ";
    *dstBuffer += reasonPhrase;
    *dstBuffer += "\r\n";
}

StringType StatusLine::toString() const
{
    BufferType buf;
    serialize(&buf);
    return buf;
}

//-------------------------------------------------------------------------------------------------
// class Request.

template<class MessageType, class MessageLineType>
bool parseRequestOrResponse(
    const ConstBufferRefType& data,
    MessageType* message,
    MessageLineType MessageType::*messageLine,
    bool parseHeadersNonStrict)
{
    enum ParseState
    {
        readingMessageLine, //request line or status line
        readingHeaders,
        readingMessageBody
    }
    state = readingMessageLine;

    int lineNumber = 0;
    for (size_t curPos = 0; curPos < data.size(); ++lineNumber)
    {
        if (state == readingMessageBody)
        {
            message->messageBody = data.mid(curPos);
            break;
        }

        //breaking into lines
        const size_t lineSepPos = nx::utils::find_first_of(
            data,
            "\r\n",
            curPos,
            data.size() - curPos);
        const ConstBufferRefType currentLine = data.mid(
            curPos,
            lineSepPos == nx::utils::BufferNpos ? lineSepPos : lineSepPos - curPos);
        switch (state)
        {
            case readingMessageLine:
                if (!(message->*messageLine).parse(currentLine))
                    return false;
                state = readingHeaders;
                break;

            case readingHeaders:
            {
                if (!currentLine.isEmpty())
                {
                    StringType headerName;
                    StringType headerValue;
                    if (!parseHeader(&headerName, &headerValue, currentLine))
                    {
                        if (parseHeadersNonStrict)
                            break;
                        else
                            return false;
                    }
                    message->headers.insert(std::make_pair(headerName, headerValue));
                    break;
                }
                else
                {
                    state = readingMessageBody;
                }
            }

            case readingMessageBody:
                break;
        }

        if (lineSepPos == nx::utils::BufferNpos)
            break;  //no more data to parse
        curPos = lineSepPos;
        ++curPos;   //skipping separator
        if (curPos < data.size() && (data[curPos] == '\r' || data[curPos] == '\n'))
            ++curPos;   //skipping complex separator (\r\n)
    }

    return true;
}

bool Request::parse(const ConstBufferRefType& data)
{
    return parseRequestOrResponse(data, this, &Request::requestLine);
}

void Request::serialize(BufferType* const dstBuffer) const
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
    dstBuffer->append((const BufferType::value_type*)"\r\n");
    dstBuffer->append(messageBody);
}

BufferType Request::serialized() const
{
    BufferType buf;
    serialize(&buf);
    return buf;
}

StringType Request::toString() const
{
    BufferType buf;
    serialize(&buf);
    return buf;
}

BufferType Request::getCookieValue(const BufferType& name) const
{
    nx::network::http::HttpHeaders::const_iterator cookieIter = headers.find("cookie");
    if (cookieIter == headers.end())
        return BufferType();

    //TODO #ak optimize string operations here
    for (const BufferType& value : cookieIter->second.split(';'))
    {
        QList<BufferType> params = value.split('=');
        if (params.size() > 1 && params[0].trimmed() == name)
            return params[1];
    }

    return BufferType();
}


//-------------------------------------------------------------------------------------------------
// class Response.

bool Response::parse(const ConstBufferRefType& data)
{
    return parseRequestOrResponse(data, this, &Response::statusLine);
}

void Response::serialize(BufferType* const dstBuffer) const
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
    dstBuffer->append((const BufferType::value_type*)"\r\n");
    dstBuffer->append(messageBody);
}

void Response::serializeMultipartResponse(
    BufferType* const dstBuffer,
    const ConstBufferRefType& boundary) const
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
    dstBuffer->append(boundary);
    dstBuffer->append("\r\n");
    serializeHeaders(headers, dstBuffer);
    dstBuffer->append((const BufferType::value_type*)"\r\n");
    dstBuffer->append(messageBody);
    dstBuffer->append("\r\n");
}


StringType Response::toString() const
{
    BufferType buf;
    serialize(&buf);
    return buf;
}

StringType Response::toMultipartString(const ConstBufferRefType& boundary) const
{
    BufferType buf;
    serializeMultipartResponse(&buf, boundary);
    return buf;
}


namespace MessageType {

QLatin1String toString(Value val)
{
    switch (val)
    {
        case request:
        {
            static QLatin1String requestStr("request");
            return requestStr;
        }
        case response:
        {
            static QLatin1String responseStr("response");
            return responseStr;
        }
        default:
        {
            static QLatin1String unknownStr("unknown");
            return unknownStr;
        }
    }
}

} // namespace MessageType

static bool isMessageBodyForbiddenByHeaders(const HttpHeaders& headers)
{
    auto contentLengthIter = headers.find("Content-Length");
    if (contentLengthIter != headers.end() &&
        contentLengthIter->second.toUInt() == 0)
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

void Message::serialize(BufferType* const dstBuffer) const
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

StringType Message::toString() const
{
    BufferType str;
    switch (type)
    {
        case MessageType::request:
            request->serialize(&str);
            break;
        case MessageType::response:
            response->serialize(&str);
            break;
        default:
            break;
    }
    return str;
}


namespace header {

const StringType kContentType = "Content-Type";
const StringType kUserAgent = "User-Agent";


namespace AuthScheme {

const char* toString( Value val )
{
    switch( val )
    {
        case basic:
            return "Basic";
        case digest:
            return "Digest";
        default:
            return "None";
    }
}

Value fromString( const char* str )
{
    if( ::strcasecmp( str, "Basic" ) == 0 )
        return basic;
    if( ::strcasecmp( str, "Digest" ) == 0 )
        return digest;
    return none;
}

Value fromString( const ConstBufferRefType& str )
{
    if( str == "Basic" )
        return basic;
    if( str == "Digest" )
        return digest;
    return none;
}

} // namespace AuthSchemeS

//-------------------------------------------------------------------------------------------------
// Authorization.

bool BasicCredentials::parse(const BufferType& str)
{
    const auto decodedBuf = BufferType::fromBase64(str);
    const int sepIndex = decodedBuf.indexOf(':');
    if (sepIndex == -1)
        return false;
    userid = decodedBuf.mid(0, sepIndex);
    password = decodedBuf.mid(sepIndex + 1);

    return true;
}

void BasicCredentials::serialize(BufferType* const dstBuffer) const
{
    BufferType serializedCredentials;
    serializedCredentials.append(userid);
    serializedCredentials.append(':');
    serializedCredentials.append(password);
    *dstBuffer += serializedCredentials.toBase64();
}

bool DigestCredentials::parse(const BufferType& str, char separator)
{
    nx::utils::parseNameValuePairs(str, separator, &params);
    auto usernameIter = params.find("username");
    if (usernameIter != params.cend())
        userid = usernameIter.value();
    return true;
}

void DigestCredentials::serialize(BufferType* const dstBuffer) const
{
    const static std::array<const char*, 5> predefinedOrder =
    {{
        "username",
        "realm",
        "nonce",
        "uri",
        "response"
    }};

    bool isFirst = true;
    auto serializeParam = [&isFirst, dstBuffer](const BufferType& name, const BufferType& value)
    {
         if (!isFirst)
            dstBuffer->append(", ");
        dstBuffer->append(name);
        dstBuffer->append("=\"");
        dstBuffer->append(value);
        dstBuffer->append("\"");
        isFirst = false;
    };

    auto params = this->params;
    for (const char* name: predefinedOrder)
    {
        auto itr = params.find(name);
        if (itr != params.end())
        {
            serializeParam(itr.key(), itr.value());
            params.erase(itr);
        }
    }
    for (auto itr = params.begin(); itr != params.end(); ++itr)
        serializeParam(itr.key(), itr.value());

}


//-------------------------------------------------------------------------------------------------
// Authorization.

const StringType Authorization::NAME("Authorization");

Authorization::Authorization():
    authScheme(AuthScheme::none)
{
    basic = NULL;
}

Authorization::Authorization(const AuthScheme::Value& authSchemeVal):
    authScheme(authSchemeVal)
{
    switch (authScheme)
    {
        case AuthScheme::basic:
            basic = new BasicCredentials();
            break;

        case AuthScheme::digest:
            digest = new DigestCredentials();
            break;

        default:
            basic = NULL;
            break;
    }
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
        case AuthScheme::basic:
            basic = new BasicCredentials(*right.basic);
            break;
        case AuthScheme::digest:
            digest = new DigestCredentials(*right.digest);
            break;
        default:
            break;
    }
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

bool Authorization::parse(const BufferType& str)
{
    clear();

    int authSchemeEndPos = str.indexOf(" ");
    if (authSchemeEndPos == -1)
        return false;

    authScheme = AuthScheme::fromString(ConstBufferRefType(str, 0, authSchemeEndPos));
    const ConstBufferRefType authParamsData(str, authSchemeEndPos + 1);
    switch (authScheme)
    {
        case AuthScheme::basic:
            basic = new BasicCredentials();
            return basic->parse(authParamsData.toByteArrayWithRawData());
        case AuthScheme::digest:
            digest = new DigestCredentials();
            return digest->parse(authParamsData.toByteArrayWithRawData());
        default:
            return false;
    }
}

void Authorization::serialize(BufferType* const dstBuffer) const
{
    dstBuffer->append(AuthScheme::toString(authScheme));
    dstBuffer->append(" ");
    if (authScheme == AuthScheme::basic)
        basic->serialize(dstBuffer);
    else if (authScheme == AuthScheme::digest)
        digest->serialize(dstBuffer);
}

BufferType Authorization::serialized() const
{
    BufferType dest;
    serialize(&dest);
    return dest;
}

void Authorization::clear()
{
    switch (authScheme)
    {
        case AuthScheme::basic:
            delete basic;
            basic = nullptr;
            break;

        case AuthScheme::digest:
            delete digest;
            digest = nullptr;
            break;

        default:
            break;
    }
    authScheme = AuthScheme::none;
}

StringType Authorization::userid() const
{
    switch (authScheme)
    {
        case AuthScheme::basic:
            return basic->userid;
        case AuthScheme::digest:
            return digest->userid;
        default:
            return StringType();
    }
}

BasicAuthorization::BasicAuthorization(
    const StringType& userName,
    const StringType& userPassword)
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

void DigestAuthorization::addParam(const BufferType& name, const BufferType& value)
{
    if (name == "username")
        digest->userid = value;

    digest->params.insert(name, value);
}


//-------------------------------------------------------------------------------------------------
// WWWAuthenticate.

const StringType WWWAuthenticate::NAME("WWW-Authenticate");

WWWAuthenticate::WWWAuthenticate(AuthScheme::Value authScheme):
    authScheme(authScheme)
{
}

bool WWWAuthenticate::parse(const BufferType& str)
{
    int authSchemeEndPos = str.indexOf(" ");
    if (authSchemeEndPos == -1)
        return false;

    authScheme = AuthScheme::fromString(ConstBufferRefType(str, 0, authSchemeEndPos));

    nx::utils::parseNameValuePairs(
        ConstBufferRefType(str, authSchemeEndPos + 1),
        ',',
        &params);

    return true;
}

void WWWAuthenticate::serialize(BufferType* const dstBuffer) const
{
    dstBuffer->append(AuthScheme::toString(authScheme));
    dstBuffer->append(" ");
    nx::utils::serializeNameValuePairs(params, dstBuffer);
}

BufferType WWWAuthenticate::serialized() const
{
    BufferType dest;
    serialize(&dest);
    return dest;
}

//-------------------------------------------------------------------------------------------------
// Accept-Encoding.

//const nx::network::http::StringType IDENTITY_CODING( "identity" );
//const nx::network::http::StringType ANY_CODING( "*" );

AcceptEncodingHeader::AcceptEncodingHeader(const nx::network::http::StringType& strValue)
{
    parse(strValue);
}

void AcceptEncodingHeader::parse(const nx::network::http::StringType& str)
{
    m_anyCodingQValue.reset();

    //TODO #ak this function is very slow. Introduce some parsing without allocations and copyings..
    auto codingsStr = str.split(',');
    for (const nx::network::http::StringType& contentCodingStr : codingsStr)
    {
        auto tokens = contentCodingStr.split(';');
        if (tokens.isEmpty())
            continue;
        double qValue = 1.0;
        if (tokens.size() > 1)
        {
            const nx::network::http::StringType& qValueStr = tokens[1].trimmed();
            if (!qValueStr.startsWith("q="))
                continue;   //bad token, ignoring...
            qValue = qValueStr.mid(2).toDouble();
        }
        const nx::network::http::StringType& contentCoding = tokens.front().trimmed();
        if (contentCoding == ANY_CODING)
            m_anyCodingQValue = qValue;
        else
            m_codings[contentCoding] = qValue;
    }
}

bool AcceptEncodingHeader::encodingIsAllowed(
    const nx::network::http::StringType& encodingName,
    double* q) const
{
    auto codingIter = m_codings.find(encodingName);
    if (codingIter == m_codings.end())
    {
        //encoding is not explicitly specified
        if (m_anyCodingQValue)
        {
            if (q)
                *q = m_anyCodingQValue.get();
            return m_anyCodingQValue.get() > 0.0;
        }

        return encodingName == IDENTITY_CODING;
    }

    if (q)
        *q = codingIter->second;
    return codingIter->second > 0.0;
}


//-------------------------------------------------------------------------------------------------
// Range.

Range::Range()
{
}

bool Range::parse(const nx::network::http::StringType& strValue)
{
    auto simpleRangeList = strValue.split(',');
    rangeSpecList.reserve(simpleRangeList.size());
    for (const StringType& simpleRangeStr : simpleRangeList)
    {
        if (simpleRangeStr.isEmpty())
            return false;
        RangeSpec rangeSpec;
        const int sepPos = simpleRangeStr.indexOf('-');
        if (sepPos == -1)
        {
            rangeSpec.start = simpleRangeStr.toULongLong();
            rangeSpec.end = rangeSpec.start;
        }
        else
        {
            rangeSpec.start = StringType::fromRawData(simpleRangeStr.constData(), sepPos).toULongLong();
            if (sepPos < simpleRangeStr.size() - 1)  //range end is not empty
            {
                rangeSpec.end = StringType::fromRawData(
                    simpleRangeStr.constData() + sepPos + 1,
                    simpleRangeStr.size() - sepPos - 1).toULongLong();
            }
        }
        if (rangeSpec.end && rangeSpec.end < rangeSpec.start)
            return false;
        rangeSpecList.push_back(std::move(rangeSpec));
    }

    return true;
}

bool Range::validateByContentSize(size_t contentSize) const
{
    for (const RangeSpec& rangeSpec : rangeSpecList)
    {
        if ((rangeSpec.start >= contentSize) || (rangeSpec.end && rangeSpec.end.get() >= contentSize))
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
            rangeSpec.end ? rangeSpec.end.get() : contentSize);
    }

    quint64 curPos = 0;
    for (const std::pair<quint64, quint64>& range: rangesSorted)
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
            rangeSpec.end ? rangeSpec.end.get() : contentSize);
    }

    quint64 curPos = 0;
    quint64 totalLength = 0;
    for (const std::pair<quint64, quint64>& range : rangesSorted)
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
        return rangeSpec.end.get() - rangeSpec.start + 1;   //both boundaries are inclusive
    else if (instanceLength)
        return instanceLength.get() - rangeSpec.start;
    else
        return 1;   //since both boundaries are inclusive, 0-0 means 1 byte (the first one)
}

StringType ContentRange::toString() const
{
    NX_ASSERT(!rangeSpec.end || (rangeSpec.end >= rangeSpec.start));

    StringType str = unitName;
    str += " ";
    str += StringType::number(rangeSpec.start) + "-";
    if (rangeSpec.end)
        str += StringType::number(rangeSpec.end.get());
    else if (instanceLength)
        str += StringType::number(instanceLength.get() - 1);
    else
        str += StringType::number(rangeSpec.start);

    if (instanceLength)
        str += "/" + StringType::number(instanceLength.get());
    else
        str += "/*";

    return str;
}


//-------------------------------------------------------------------------------------------------
// Via.

bool Via::parse(const nx::network::http::StringType& strValue)
{
    if (strValue.isEmpty())
        return true;

    //introducing loop counter to guarantee method finiteness in case of bug in code
    for (size_t curEntryEnd = nx::utils::find_first_of(strValue, ","), curEntryStart = 0, i = 0;
        curEntryStart != nx::utils::BufferNpos && (i < 1000);
        curEntryStart = (curEntryEnd == nx::utils::BufferNpos ? curEntryEnd : curEntryEnd + 1), curEntryEnd = nx::utils::find_first_of(strValue, ",", curEntryEnd + 1), ++i)
    {
        ProxyEntry entry;

        //skipping spaces at the start of entry
        while ((curEntryStart < (curEntryEnd == nx::utils::BufferNpos ? strValue.size() : curEntryEnd)) &&
            (strValue.at(curEntryStart) == ' '))
        {
            ++curEntryStart;
        }

        //curEntryStart points first char after comma
        size_t receivedProtoEnd = nx::utils::find_first_of(strValue, " ", curEntryStart);
        if (receivedProtoEnd == nx::utils::BufferNpos)
            return false;
        ConstBufferRefType protoNameVersion(strValue, curEntryStart, receivedProtoEnd - curEntryStart);
        size_t nameVersionSep = nx::utils::find_first_of(protoNameVersion, "/");
        if (nameVersionSep == nx::utils::BufferNpos)
        {
            //only version present
            entry.protoVersion = protoNameVersion;
        }
        else
        {
            entry.protoName = protoNameVersion.mid(0, nameVersionSep);
            entry.protoVersion = protoNameVersion.mid(nameVersionSep + 1);
        }

        size_t receivedByStart = nx::utils::find_first_not_of(strValue, " ", receivedProtoEnd + 1);
        if (receivedByStart == nx::utils::BufferNpos || receivedByStart > curEntryEnd)
            return false;   //no receivedBy field

        size_t receivedByEnd = nx::utils::find_first_of(strValue, " ", receivedByStart);
        if (receivedByEnd == nx::utils::BufferNpos || (receivedByEnd > curEntryEnd))
        {
            receivedByEnd = curEntryEnd;
        }
        else
        {
            //comment present
            size_t commentStart = nx::utils::find_first_not_of(strValue, " ", receivedByEnd + 1);
            if (commentStart != nx::utils::BufferNpos && commentStart < curEntryEnd)
                entry.comment = strValue.mid(commentStart, curEntryEnd == nx::utils::BufferNpos ? -1 : (curEntryEnd - commentStart)); //space are allowed in comment
        }
        entry.receivedBy = strValue.mid(receivedByStart, receivedByEnd - receivedByStart);

        entries.push_back(entry);
    }

    return true;
}

StringType Via::toString() const
{
    StringType result;

    //TODO #ak estimate required buffer size and allocate in advance

    for (const ProxyEntry& entry : entries)
    {
        if (!result.isEmpty())
            result += ", ";

        if (entry.protoName)
        {
            result += entry.protoName.get();
            result += "/";
        }
        result += entry.protoVersion;
        result += ' ';
        result += entry.receivedBy;
        if (!entry.comment.isEmpty())
        {
            result += ' ';
            result += entry.comment;
        }
    }
    return result;
}


//-------------------------------------------------------------------------------------------------
// Keep-Alive.

KeepAlive::KeepAlive():
    timeout(0)
{
}

KeepAlive::KeepAlive(
    std::chrono::seconds _timeout,
    boost::optional<int> _max)
    :
    timeout(std::move(_timeout)),
    max(std::move(_max))
{
}

bool KeepAlive::parse(const nx::network::http::StringType& strValue)
{
    max.reset();

    const auto params = strValue.split(',');
    bool timeoutFound = false;
    for (auto param : params)
    {
        param = param.trimmed();
        const int sepPos = param.indexOf('=');
        if (sepPos == -1)
            continue;
        const auto paramName = ConstBufferRefType(param, 0, sepPos);
        const auto paramValue = ConstBufferRefType(param, sepPos + 1).trimmed();

        if (paramName == "timeout")
        {
            timeout = std::chrono::seconds(paramValue.toUInt());
            timeoutFound = true;
        }
        else if (paramName == "max")
        {
            max = paramValue.toUInt();
        }
    }

    return timeoutFound;
}

StringType KeepAlive::toString() const
{
    StringType result = "timeout=" + StringType::number((unsigned int)timeout.count());
    if (max)
        result += ", max=" + StringType::number(*max);
    return result;
}

//-------------------------------------------------------------------------------------------------
// Server

// TODO: #ak Better replace following macro with constants.

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

const StringType Server::NAME("Server");

//-------------------------------------------------------------------------------------------------
// Server::Product
bool Server::Product::operator==(const Server::Product& right) const
{
    return name == right.name
        && version == right.version
        && comment == right.comment;
}

//-------------------------------------------------------------------------------------------------
// Server

Server::Server()
{
    products.push_back(Product{
        nx::utils::AppInfo::productNameShort().toUtf8(),
        nx::utils::SoftwareVersion(nx::utils::AppInfo::applicationVersion()),
        nx::utils::AppInfo::organizationName().toUtf8() });
}

bool Server::operator==(const Server& right) const
{
    return products == right.products;
}

bool Server::parse(const nx::network::http::StringType& serverString)
{
    // "Nx/1.0 Mozilla/5.0 (Windows NT 6.1; WOW64)"

    products.clear();

    QnByteArrayConstRef inputStr(serverString);
    while (!inputStr.isEmpty())
    {
        products.push_back(Product());
        Product& product = products.back();
        if (!readProduct(&product, &inputStr))
            return false;
    }

    return !products.empty();
}

StringType Server::toString() const
{
    StringType result;
    for (const auto& product : products)
    {
        result += toString(product);
        result += " ";
    }

    // TODO: #ak Remove COMPATIBILITY_SERVER_STRING from here.
    return result + COMPATIBILITY_SERVER_STRING;
}

nx::network::http::StringType Server::toString(const Server::Product& product) const
{
    StringType result;
    result += product.name;
    if (product.version)
        result += "/" + product.version->toString().toUtf8();
    if (!product.comment.isEmpty())
        result += " (" + product.comment + ")";

    return result;
}

bool Server::readProduct(Server::Product* product, QnByteArrayConstRef* inputStr)
{
    readProductName(product, inputStr);
    if (inputStr->isEmpty())
        return true;
    if (product->name.isEmpty())
        return false; // See [rfc2616, 3.8].

    readProductVersion(product, inputStr);
    if (inputStr->isEmpty())
        return true;

    readProductComment(product, inputStr);
    return true;
}

void Server::readProductName(Server::Product* product, QnByteArrayConstRef* inputStr)
{
    while (!inputStr->isEmpty() && inputStr->front() == ' ')
        inputStr->pop_front();

    auto len = nx::utils::find_first_of(*inputStr, "/ (");
    product->name = inputStr->mid(0, len);
    inputStr->pop_front(len);
}

void Server::readProductVersion(Server::Product* product, QnByteArrayConstRef* inputStr)
{
    if (inputStr->front() != '/')
        return; //< No version here.

    inputStr->pop_front();
    if (inputStr->isEmpty())
        return; //< No version here.

    const auto len = nx::utils::find_first_of(*inputStr, " (");
    if (len == 0)
        return; //< No version here.

    product->version = nx::utils::SoftwareVersion(inputStr->mid(0, len));
    inputStr->pop_front(len);
}

void Server::readProductComment(Server::Product* product, QnByteArrayConstRef* inputStr)
{
    while (!inputStr->isEmpty() && inputStr->front() == ' ')
        inputStr->pop_front();

    if (inputStr->isEmpty() || inputStr->front() != '(')
        return; //< No comment here.
    inputStr->pop_front();

    const auto commentEnd = nx::utils::find_first_of(*inputStr, ")");
    product->comment = inputStr->mid(0, commentEnd);
    inputStr->pop_front(commentEnd);
    if (!inputStr->isEmpty())
        inputStr->pop_front(); //< Removing ")"
}

//-------------------------------------------------------------------------------------------------

const StringType StrictTransportSecurity::NAME("Strict-Transport-Security");

bool StrictTransportSecurity::operator==(const StrictTransportSecurity& rhs) const
{
    return maxAge == rhs.maxAge
        && includeSubDomains == rhs.includeSubDomains
        && preload == rhs.preload;
}

bool StrictTransportSecurity::parse(const nx::network::http::StringType& strValue)
{
    const auto nameValueDictionary = nx::utils::parseNameValuePairs(strValue, ';');
    const auto maxAgeIter = nameValueDictionary.find("max-age");
    if (maxAgeIter == nameValueDictionary.end())
        return false;
    maxAge = std::chrono::seconds(maxAgeIter->toInt());
    includeSubDomains = nameValueDictionary.contains("includeSubDomains");
    preload = nameValueDictionary.contains("preload");
    // Ignoring unknown attributes.
    return true;
}

StringType StrictTransportSecurity::toString() const
{
    StringType result = lm("max-age=%1").arg(maxAge.count()).toUtf8();
    if (includeSubDomains)
        result += ";includeSubDomains";
    if (preload)
        result += ";preload";
    return result;
}

} // namespace header

//-------------------------------------------------------------------------------------------------

ChunkHeader::ChunkHeader():
    chunkSize(0)
{
}

void ChunkHeader::clear()
{
    chunkSize = 0;
    extensions.clear();
}

int ChunkHeader::parse(const ConstBufferRefType& buf)
{
    const char* curPos = buf.constData();
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
                        BufferType(extNameStart, curPos - extNameStart),
                        BufferType()));
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
                        BufferType(extNameStart, extSepPos - extNameStart),
                        BufferType(extValStart, extValSize)));
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
                        BufferType(extNameStart, curPos - extNameStart),
                        BufferType()));
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
                        BufferType(extNameStart, extSepPos - extNameStart),
                        BufferType(extValStart, extValSize)));
                }

                if ((dataEnd - curPos < 2) || *(curPos + 1) != '\n')
                    return -1;
                return curPos + 2 - buf.constData();

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

int ChunkHeader::serialize(BufferType* const /*dstBuffer*/) const
{
    //TODO #ak
    return -1;
}


static const StringType defaultUserAgentString = lit("%1%2/%3 (%4) %5").arg(
    nx::utils::AppInfo::productNameLong(),
    PRODUCT_NAME_SUFFIX,
    nx::utils::AppInfo::applicationVersion(),
    nx::utils::AppInfo::organizationName(),
    COMMON_USER_AGENT
).toUtf8();

StringType userAgentString()
{
    return defaultUserAgentString;
}

static const StringType defaultServerString = lit("%1/%2 (%3) %4").arg(
    nx::utils::AppInfo::productNameLong(),
    nx::utils::AppInfo::applicationVersion(),
    nx::utils::AppInfo::organizationName(),
    COMPATIBILITY_SERVER_STRING
).toUtf8();

StringType serverString()
{
    return defaultServerString;
}

static const char* weekDaysStr[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

QByteArray formatDateTime(const QDateTime& value)
{
    static const int SECONDS_PER_MINUTE = 60;
    static const int SECONDS_PER_HOUR = 3600;

    if (value.isNull() || !value.isValid())
        return QByteArray();

    const QDate& date = value.date();
    const QTime& time = value.time();
    const int offsetFromUtcSeconds = value.offsetFromUtc();
    const int offsetFromUtcHHMM =
        (offsetFromUtcSeconds / SECONDS_PER_HOUR) * 100 +
        (offsetFromUtcSeconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;

    char strDateBuf[256];
    sprintf(strDateBuf, "%s, %02d %s %d %02d:%02d:%02d %+05d",
        weekDaysStr[date.dayOfWeek() - 1],
        date.day(),
        months[date.month() - 1],
        date.year(),
        time.hour(),
        time.minute(),
        time.second(),
        offsetFromUtcHHMM);

    return QByteArray(strDateBuf);
}

} // namespace nx
} // namespace network
} // namespace http
