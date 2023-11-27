// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <compare>
#include <functional>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <QtCore/QDateTime>

#include <nx/network/socket_common.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/buffer.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/software_version.h>
#include <nx/utils/std/cpp20.h>
#include <nx/utils/stree/attribute_dictionary.h>
#include <nx/utils/string.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/url.h>

/**
 * Holds HTTP implementation suitable for async/sync i/o.
 * Common structures, parsers/serializers, http client, http server.
 *
 * All classes use std::string for representing HTTP headers and nx::Buffer wherever
 * a raw buffer is needed.
 */
namespace nx::network::http {

const int DEFAULT_HTTP_PORT = 80;
const int DEFAULT_HTTPS_PORT = 443;

static constexpr char kUrlSchemeName[] = "http";
static constexpr char kSecureUrlSchemeName[] = "https";

NX_NETWORK_API const char* urlScheme(bool isSecure);

NX_NETWORK_API bool isUrlScheme(const std::string_view& scheme);

// Emulating explicit isUrlScheme(const QString&).
template<typename S, typename = std::enable_if_t<std::is_same_v<S, QString>>>
bool isUrlScheme(const S& s)
{
    return isUrlScheme(s.toStdString());
}

NX_NETWORK_API int defaultPortForScheme(const std::string_view& scheme);
NX_NETWORK_API int defaultPort(bool isSecure);

/** HTTP header container.
 * WARNING: This is multimap(!) to allow same header be present multiple times in a
 * single http message.
 * To insert or replace use nx::network::http::insertOrReplaceHeader
 */
using HttpHeaders = std::multimap<std::string, std::string, nx::utils::ci_less>;
using HttpHeader = HttpHeaders::value_type;

/** map<name, value>. */
class RequestPathParams:
    public nx::utils::stree::StringAttrDict
{
    using base_type = nx::utils::stree::StringAttrDict;

public:
    using base_type::base_type;

    std::string getByName(const std::string& name) const
    {
        auto it = find(name);
        return it != end() ? it->second : std::string();
    }
};

/**
 * This is convenient method for simplify transition from QHttp.
 * @return Value of header headerName (if found), empty string otherwise.
*/
NX_NETWORK_API std::string getHeaderValue(
    const HttpHeaders& headers, const std::string_view& headerName);

/**
 * @return false if requested header was not found.
 */
NX_NETWORK_API bool readHeader(
    const HttpHeaders& headers,
    const std::string_view& headerName,
    int* value);

/**
 * Convenient function for inserting or replacing header.
 * @return iterator of added element.
 */
NX_NETWORK_API HttpHeaders::iterator insertOrReplaceHeader(
    HttpHeaders* const headers, const HttpHeader& newHeader);

template<typename HeaderType>
HttpHeaders::iterator insertOrReplaceHeader(
    HttpHeaders* const headers, const HeaderType& header)
{
    return insertOrReplaceHeader(
        headers,
        HttpHeader(HeaderType::NAME, header.toString()));
}

NX_NETWORK_API HttpHeaders::iterator insertHeader(
    HttpHeaders* const headers, HttpHeader newHeader);

NX_NETWORK_API void removeHeader(
    HttpHeaders* const headers, const std::string& name);

/**
 * Adds CORS to headers. supportedOrigins must be equal to '*' or a comma separated origins
 * supported.
 */
NX_NETWORK_API void insertOrReplaceCorsHeaders(
    HttpHeaders* headers,
    const class Method& method,
    std::string origin,
    const std::string& supportedOrigins,
    std::string_view methods);

/** Parses data and saves header name and data to *headerName and *headerValue. */
NX_NETWORK_API bool parseHeader(
    const ConstBufferRefType& data,
    std::string* const headerName,
    std::string* const headerValue);

NX_NETWORK_API bool parseHeader(
    const ConstBufferRefType& data,
    ConstBufferRefType* const headerName,
    ConstBufferRefType* const headerValue);

NX_NETWORK_API HttpHeader parseHeader(const ConstBufferRefType& data);

template<class MessageType, class MessageLineType>
bool parseRequestOrResponse(
    const ConstBufferRefType& data,
    MessageType* message,
    MessageLineType MessageType::* messageLine,
    bool parseHeadersNonStrict = false)
{
    // TODO: nx::utils::split

    enum ParseState
    {
        readingMessageLine, //request line or status line
        readingHeaders,
        readingMessageBody
    }
    state = readingMessageLine;

    for (size_t curPos = 0; curPos < data.size(); /*no increment*/)
    {
        if (state == readingMessageBody)
        {
            message->messageBody = data.substr(curPos);
            break;
        }

        //breaking into lines
        const auto lineSepPos = data.find_first_of("\r\n", curPos);
        const ConstBufferRefType currentLine = data.substr(
            curPos,
            lineSepPos == ConstBufferRefType::npos ? lineSepPos : lineSepPos - curPos);
        switch (state)
        {
            case readingMessageLine:
                if (!(message->*messageLine).parse(currentLine))
                    return false;
                state = readingHeaders;
                break;

            case readingHeaders:
            {
                if (!currentLine.empty())
                {
                    std::string headerName;
                    std::string headerValue;
                    if (!parseHeader(currentLine, &headerName, &headerValue))
                    {
                        if (parseHeadersNonStrict)
                            break;
                        else
                            return false;
                    }
                    message->headers.emplace(std::move(headerName), std::move(headerValue));
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

        if (lineSepPos == ConstBufferRefType::npos)
            break;  //no more data to parse
        curPos = lineSepPos;
        ++curPos;   //skipping separator
        if (curPos < data.size() && (data[lineSepPos] == '\r' && data[curPos] == '\n'))
            ++curPos;   //skipping complex separator (\r\n)
    }

    return true;
}

namespace StatusCode {

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

} // namespace StatusCode

//-------------------------------------------------------------------------------------------------
// Method.

class NX_NETWORK_API Method
{
public:
    static constexpr std::string_view connect = "CONNECT";
    static constexpr std::string_view get = "GET";
    static constexpr std::string_view head = "HEAD";
    static constexpr std::string_view post = "POST";
    static constexpr std::string_view put = "PUT";
    static constexpr std::string_view patch = "PATCH";
    static constexpr std::string_view delete_ = "DELETE";
    static constexpr std::string_view options = "OPTIONS";

    Method() = default;
    Method(const Method&) = default;
    Method(Method&&) = default;

    Method(const std::string_view& str);
    Method(const char* str);
    Method(std::string str);

    Method& operator=(const Method&) = default;
    Method& operator=(Method&&) = default;

    Method& operator=(const std::string_view& str);
    Method& operator=(const char* str);
    Method& operator=(std::string str);

    bool operator<(const Method& right) const;
    bool operator<(const std::string_view& right) const;

    bool operator==(const Method& right) const;
    bool operator!=(const Method& right) const;

    inline bool operator==(const char* right) const
    { return nx::utils::stricmp(m_value, right) == 0; }

    inline bool operator!=(const char* right) const
    { return nx::utils::stricmp(m_value, right) != 0; }

    inline bool operator==(const std::string& right) const
    { return nx::utils::stricmp(m_value, right) == 0; }

    inline bool operator!=(const std::string& right) const
    { return nx::utils::stricmp(m_value, right) != 0; }

    inline bool operator==(const std::string_view& right) const
    { return nx::utils::stricmp(m_value, right) == 0; }

    inline bool operator!=(const std::string_view& right) const
    { return nx::utils::stricmp(m_value, right) != 0; }

    const std::string& toString() const;

    static bool isKnown(const std::string_view& str);

    static bool isMessageBodyAllowed(const Method& method);
    bool isMessageBodyAllowed() const
    {
        return isMessageBodyAllowed(*this);
    }

    static bool isMessageBodyAllowedInResponse(
        const Method& method,
        StatusCode::Value statusCode);

private:
    std::string m_value;
};

inline bool operator==(const std::string_view& left, const Method& right)
{ return right == left; }

inline bool operator!=(const std::string_view& left, const Method& right)
{ return right != left; }

inline std::ostream& operator<<(std::ostream& os, const Method& method)
{ return os << method.toString(); }

//-------------------------------------------------------------------------------------------------

/**
 * Represents string like HTTP/1.1, RTSP/1.0.
 */
class NX_NETWORK_API MimeProtoVersion
{
public:
    std::string protocol;
    std::string version;

    bool parse(const ConstBufferRefType& data);

    /** Appends serialized data to dstBuffer. */
    void serialize(nx::Buffer* const dstBuffer) const;

    bool operator==(const MimeProtoVersion& right) const
    {
        return protocol == right.protocol
            && version == right.version;
    }

    bool operator!=(const MimeProtoVersion& right) const
    {
        return !(*this == right);
    }

    friend bool operator < (const MimeProtoVersion& lhs, const MimeProtoVersion& rhs)
    {
        return lhs.version < rhs.version;
    }
};

static const MimeProtoVersion http_1_0 = { "HTTP", "1.0" };
static const MimeProtoVersion http_1_1 = { "HTTP", "1.1" };

class NX_NETWORK_API RequestLine
{
public:
    Method method;

    // FIXME: URL is not correct structure here, should use more general structure.
    //        See: RFC 2616, section-5.1.2.
    nx::utils::Url url;
    MimeProtoVersion version;

    bool parse(const ConstBufferRefType& data);
    /** Appends serialized data to dstBuffer. */
    void serialize(nx::Buffer* dstBuffer) const;
    std::string toString() const;

    enum class EncodeUrlParts { all, authority };

    /**
     * Encodes the URL before putting it into the HTTP request.
     */
    std::string encodeUrl(
        const nx::utils::Url& url, EncodeUrlParts parts = EncodeUrlParts::all) const;
};

inline bool operator==(const RequestLine& left, const RequestLine& right)
{
    return left.method == right.method
        && left.url == right.url
        && left.version == right.version;
}

class NX_NETWORK_API StatusLine
{
public:
    MimeProtoVersion version;
    StatusCode::Value statusCode;
    std::string reasonPhrase;

    StatusLine();
    StatusLine(StatusLine&&) = default;
    StatusLine& operator=(StatusLine&&) = default;
    StatusLine(const StatusLine&) = default;
    StatusLine& operator=(const StatusLine&) = default;

    bool parse(const ConstBufferRefType& data);
    /** Appends serialized data to dstBuffer. */
    void serialize(nx::Buffer* const dstBuffer) const;
    std::string toString() const;
};

inline bool operator==(const StatusLine& left, const StatusLine& right)
{
    return left.version == right.version
        && left.statusCode == right.statusCode
        && left.reasonPhrase == right.reasonPhrase;
}

NX_NETWORK_API void serializeHeaders(const HttpHeaders& headers, nx::Buffer* dstBuffer);

static constexpr char kDeletedCookieValue[] = "_DELETED_COOKIE_VALUE_";

class NX_NETWORK_API Request
{
public:
    RequestLine requestLine;
    HttpHeaders headers;
    nx::Buffer messageBody;

    bool parse(const ConstBufferRefType& data);

    /**
     * Appends serialized data to dstBuffer.
     * NOTE: Adds \r\n headers/body separator.
     */
    void serialize(nx::Buffer* const dstBuffer) const;

    nx::Buffer serialized() const;
    std::string toString() const;

    /**
     * @param Case-sensitive cookie name.
     */
    std::string getCookieValue(const std::string_view& name) const;

    /**
     * @param Case-sensitive cookie name.
     */
    void removeCookie(const std::string_view& name);
};

inline bool operator==(const Request& left, const Request& right)
{
    return left.requestLine == right.requestLine
        && left.headers == right.headers
        && left.messageBody == right.messageBody;
}

inline std::ostream& operator<<(std::ostream& os, const Request& request)
{
    return os << request.toString();
}

class NX_NETWORK_API Response
{
public:
    StatusLine statusLine;
    HttpHeaders headers;
    nx::Buffer messageBody;

    bool parse(const ConstBufferRefType& data);

    /** Appends serialized data to dstBuffer. */
    void serialize(nx::Buffer* dstBuffer) const;

    /** Appends serialized multipart data block to dstBuffer. */
    void serializeMultipartResponse(
        const ConstBufferRefType& boundary,
        nx::Buffer* dstBuffer) const;

    std::string toString() const;
    std::string toMultipartString(const ConstBufferRefType& boundary) const;

    void setCookie(
        const std::string& name, const std::string& value,
        const std::string& path = "/", bool secure = true);

    void setDeletedCookie(const std::string& name);
    std::map<std::string, std::string> getCookies() const;
};

NX_NETWORK_API HttpHeader deletedCookieHeader(
    const std::string& name, const std::string& path = "/");

NX_NETWORK_API HttpHeader cookieHeader(
    const std::string& name,
    const std::string& value,
    const std::string& path = "/",
    bool secure = true,
    bool httpOnly = true);

inline bool operator==(const Response& left, const Response& right)
{
    return left.statusLine == right.statusLine
        && left.headers == right.headers
        && left.messageBody == right.messageBody;
}

inline std::ostream& operator<<(std::ostream& os, const Response& response)
{
    return os << response.toString();
}

NX_NETWORK_API bool isMessageBodyPresent(const Response& response);

class NX_NETWORK_API RtspResponse:
    public Response
{
public:
    bool parse(const ConstBufferRefType& data);
};

namespace MessageType {

enum Value
{
    none,
    request,
    response
};

NX_NETWORK_API const char* toString(Value val);

} // namespace MessageType

class NX_NETWORK_API Message
{
public:
    MessageType::Value type;
    union
    {
        Request* request;
        Response* response;
    };

    Message(MessageType::Value _type = MessageType::none);
    Message(const Message& right);
    Message(Message&& right);
    ~Message();

    Message& operator=(const Message& right);
    Message& operator=(Message&& right);

    void serialize(nx::Buffer* const dstBuffer) const;

    void clear();

    HttpHeaders& headers()
    {
        return type == MessageType::request ? request->headers : response->headers;
    }

    const HttpHeaders& headers() const
    {
        return type == MessageType::request ? request->headers : response->headers;
    }

    const MimeProtoVersion& version() const
    {
        return type == MessageType::request
            ? request->requestLine.version
            : response->statusLine.version;
    }

    void setBody(nx::Buffer body);

    std::string toString() const;
    bool operator==(const Message& msg) const;
};

NX_REFLECTION_ENUM_CLASS(AuthType,
    authBasicAndDigest,
    authDigest,
    authBasic
)

/** Contains HTTP header structures. */
namespace header {

/** Common header name constants. */
static constexpr char kAccept[] = "Accept";
static constexpr char kAcceptLanguage[] = "Accept-Language";
static constexpr char kContentType[] = "Content-Type";
static constexpr char kContentLength[] = "Content-Length";
static constexpr char kUserAgent[] = "User-Agent";

//!Http authentication scheme enumeration
namespace AuthScheme {

enum Value
{
    none,
    basic,
    digest,
    bearer,
};

NX_NETWORK_API std::string_view toString(Value val);
NX_NETWORK_API Value fromString(const std::string_view& str);

} // namespace AuthScheme

/**
 * Login/password to use in http authorization.
 */
class NX_NETWORK_API UserCredentials
{
public:
    std::string userid;
    std::string password;

    auto operator<=>(const UserCredentials&) const = default;
};

/** rfc2617, section 2. */
class NX_NETWORK_API BasicCredentials:
    public UserCredentials
{
public:
    bool parse(const std::string_view& str);
    void serialize(nx::Buffer* dest) const;
};

/** rfc2617, section 3.2.2. */
class NX_NETWORK_API DigestCredentials:
    public UserCredentials
{
public:
    std::map<std::string, std::string> params;

    bool parse(const std::string_view& str, char separator = ',');
    void serialize(nx::Buffer* dest) const;
};

/** rfc6750, section 2.1. */
class NX_NETWORK_API BearerCredentials:
    public UserCredentials
{
public:
    std::string token;

    bool parse(const std::string_view& str);
    void serialize(nx::Buffer* const dstBuffer) const;
};

/** Authorization header ([rfc2616, 14.8], rfc2617). */
class NX_NETWORK_API Authorization
{
public:
    static constexpr char NAME[] = "Authorization";

    AuthScheme::Value authScheme;
    union
    {
        BasicCredentials* basic;
        DigestCredentials* digest;
        BearerCredentials* bearer;
    };

    Authorization();
    Authorization(const AuthScheme::Value& authSchemeVal);
    Authorization(Authorization&& right);
    Authorization(const Authorization&);
    ~Authorization();

    Authorization& operator=(Authorization&& right);

    bool parse(const std::string_view& str);
    void serialize(nx::Buffer* const dest) const;
    nx::Buffer serialized() const;
    std::string toString() const;
    void clear();

    std::string userid() const;

private:
    const Authorization& operator=(const Authorization&);
};

static constexpr char kProxyAuthorization[] = "Proxy-Authorization";

/**
 * Convenient class for generating Authorization header with Basic authentication method.
 */
class NX_NETWORK_API BasicAuthorization:
    public Authorization
{
public:
    BasicAuthorization(const std::string& userName, const std::string& userPassword);
};

class NX_NETWORK_API DigestAuthorization:
    public Authorization
{
public:
    DigestAuthorization();
    DigestAuthorization(DigestAuthorization&& right);
    DigestAuthorization(const DigestAuthorization& right);

    void addParam(const std::string& name, const std::string& value);
};

class NX_NETWORK_API BearerAuthorization:
    public Authorization
{
public:
    BearerAuthorization(const std::string& token);
};

/**
 * [rfc2616, 14.47].
 */
class NX_NETWORK_API WWWAuthenticate
{
public:
    static constexpr char NAME[] = "WWW-Authenticate";

    AuthScheme::Value authScheme;
    std::map<std::string, std::string> params;

    WWWAuthenticate(AuthScheme::Value authScheme = AuthScheme::none);

    std::string getParam(const std::string& key) const;
    bool parse(const std::string_view& str);
    void serialize(nx::Buffer* dest) const;
    nx::Buffer serialized() const;
    std::string toString() const;
};

static constexpr char IDENTITY_CODING[] = "identity";
static constexpr char ANY_CODING[] = "*";

static constexpr char kProxyAuthenticate[] = "Proxy-Authenticate";

/**
 * [rfc2616, 14.3].
 */
class NX_NETWORK_API AcceptEncodingHeader
{
public:
    AcceptEncodingHeader(const std::string_view& str);

    void parse(const std::string_view& str);

    /**
     * @return true if encodingName is present in header and
     *   returns corresponding qvalue in *q (if not null).
     */
    bool encodingIsAllowed(
        const std::string& encodingName,
        double* q = nullptr) const;

    const std::map<std::string, double>& allEncodings() const;

private:
    /** map<coding, qvalue>. */
    std::map<std::string, double> m_codings;
    std::optional<double> m_anyCodingQValue;
};

/**
 * NOTE: Boundaries are inclusive.
 */
class NX_NETWORK_API RangeSpec
{
public:
    quint64 start = 0;
    std::optional<quint64> end;
};

/**
 * [rfc2616, 14.35].
 */
class NX_NETWORK_API Range
{
public:
    Range();

    /**
     * NOTE: In case of parse error, contents of this object are undefined.
     */
    bool parse(const std::string_view& str);

    /**
     * @return true if range is satisfiable for content of size contentSize.
     */
    bool validateByContentSize(size_t contentSize) const;
    /**
     * @return true, if empty range (does not include any bytes of content).
     */
    bool empty() const;
    /**
     * @return true, if range is full (includes all bytes of content of size contentSize).
     */
    bool full(size_t contentSize) const;
    /**
     * @return range length for content of size contentSize.
     */
    quint64 totalRangeLength(size_t contentSize) const;

    std::vector<RangeSpec> rangeSpecList;
};

/**
 * [rfc2616, 14.16].
 */
class NX_NETWORK_API ContentRange
{
public:
    //!By default, bytes
    std::string unitName;
    std::optional<quint64> instanceLength;
    RangeSpec rangeSpec;

    ContentRange();

    quint64 rangeLength() const;
    std::string toString() const;
};

/**
 * [rfc2616, 14.45].
 */
class NX_NETWORK_API Via
{
public:
    class ProxyEntry
    {
    public:
        std::optional<std::string> protoName;
        std::string protoVersion;
        /** ( host [ ":" port ] ) | pseudonym. */
        std::string receivedBy;
        std::string comment;
    };

    std::vector<ProxyEntry> entries;

    /**
     * NOTE: In case of parse error, contents of this object are undefined.
     */
    bool parse(const std::string_view& str);
    std::string toString() const;
};

class NX_NETWORK_API KeepAlive
{
public:
    std::chrono::seconds timeout = std::chrono::seconds::zero();
    std::optional<int> max;

    KeepAlive() = default;
    KeepAlive(
        std::chrono::seconds _timeout,
        std::optional<int> _max = std::nullopt);

    bool parse(const std::string_view& str);
    std::string toString() const;
};

class NX_NETWORK_API Server
{
public:
    class Product
    {
    public:
        std::string name;
        std::string version;
        std::string comment;

        std::string toString() const;
        bool operator==(const Product&) const;

        /**
         * @param str String of the following format:
         * product = token ["/" product-version] *( RWS comment )
         * product-version = token
         * comment = "(" TEXT ")"
         */
        static Product fromString(const std::string_view& str);
    };

    static constexpr char NAME[] = "Server";

    std::vector<Product> products;

    Server();

    bool operator==(const Server&) const;

    bool parse(const std::string_view& strValue);
    std::string toString() const;
};

// User-Agent header has the same format as Server.
using UserAgent = Server;

class NX_NETWORK_API StrictTransportSecurity
{
public:
    static constexpr char NAME[] = "Strict-Transport-Security";

    std::chrono::seconds maxAge = std::chrono::seconds::zero();
    bool includeSubDomains = false;
    bool preload = false;

    bool operator==(const StrictTransportSecurity&) const;
    bool parse(const std::string_view& strValue);
    std::string toString() const;
};

class NX_NETWORK_API XForwardedFor
{
public:
    static constexpr char NAME[] = "X-Forwarded-For";

    std::string client;
    std::vector<std::string> proxies;

    bool parse(const std::string_view& str);
    std::string toString() const;
};

class NX_NETWORK_API XForwardedProto
{
public:
    static constexpr char NAME[] = "X-Forwarded-Proto";

    static constexpr char kHttp[] = "http";
    static constexpr char kHttps[] = "https";
};

struct NX_NETWORK_API ForwardedElement
{
    std::string by;
    std::string for_;
    std::string host;
    std::string proto;

    bool parse(const std::string_view& str);
    std::string toString() const;

    bool operator==(const ForwardedElement& right) const;

private:
    std::string quoteIfNeeded(const std::string_view& str) const;
};

class NX_NETWORK_API Forwarded
{
public:
    static constexpr char NAME[] = "Forwarded";

    std::vector<ForwardedElement> elements;

    Forwarded() = default;
    Forwarded(std::vector<ForwardedElement> elements);

    bool parse(const std::string_view& str);
    std::string toString() const;

    bool operator==(const Forwarded& right) const;
};

struct NX_NETWORK_API ContentType
{
    static constexpr char NAME[] = "Content-Type";

    static constexpr char kAny[] = "*/*";
    static constexpr char kDefaultCharset[] = "utf-8";

    static const ContentType kPlain;
    static const ContentType kHtml;
    static const ContentType kXml;
    static const ContentType kForm;
    static const ContentType kJson;
    static const ContentType kUbjson;
    static const ContentType kBinary;

    std::string value;
    std::string charset;

    ContentType(const std::string_view& str = kPlain.toString());
    ContentType(const char* str);

    /**
     * NOTE: This constructor works with any type that has "const char* data()" and "size()".
     * E.g., QByteArray.
     */
    template<typename String>
    ContentType(
        const String& str,
        std::enable_if_t<nx::utils::IsConvertibleToStringViewV<String>>* = nullptr)
        :
        ContentType(std::string_view(str.data(), (std::size_t) str.size()))
    {
    }

    std::string toString() const;

    bool operator==(const ContentType& rhs) const;
    bool operator!=(const ContentType& rhs) const { return !(*this == rhs); }
    bool operator==(const std::string& rhs) const;

    operator std::string() const { return toString(); }
};

class NX_NETWORK_API Host
{
public:
    static constexpr char NAME[] = "Host";

    Host(const SocketAddress& endpoint);

    /**
     * Always omits default HTTP ports (80, 443).
     */
    std::string toString() const;

private:
    SocketAddress m_endpoint;
};

} // namespace header

using ChunkExtension = std::pair<std::string, std::string>;

/**
 * chunk-size [ chunk-extension ] CRLF
 * chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
 * chunk-ext-name = token
 * chunk-ext-val  = token | quoted-string
 */
class NX_NETWORK_API ChunkHeader
{
public:
    size_t chunkSize = 0;
    std::vector<ChunkExtension> extensions;

    void clear();

    /**
     * @return bytes read from buf. -1 in case of parse error.
     * NOTE: In case of parse error object state is indefined.
     */
    int parse(const ConstBufferRefType& buf);

    /**
     * @return bytes written to dest. -1 in case of serialize error.
     *   In this case contents of dest are undefined.
     */
    int serialize(nx::Buffer* const dest) const;
};

/**
 * @return common value for User-Agent header.
 */
NX_NETWORK_API std::string userAgentString();

NX_NETWORK_API header::UserAgent defaultUserAgent();

/**
 * @return common value for Server header.
 */
NX_NETWORK_API std::string serverString();

/**
 * Convert QDateTime to HTTP header date format according to RFC 1123#5.2.14.
 */
NX_NETWORK_API std::string formatDateTime(const QDateTime& value);

/**
 * Parses the http Date according to RFC 2616#3.3 Date/Time Formats.
 *
 * NOTE: There are 3 different formats, though 2 and 3 are deprecated by HTTP 1.1 standard
 * The order in which parsing is attempted is:
 * 1. Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 * 2. Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850
 * 3. Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 */
NX_NETWORK_API QDateTime parseDate(const std::string_view& str);

} // namespace nx::network::http

template<>
struct std::hash<nx::network::http::Method>
{
    std::size_t operator()(const nx::network::http::Method& method) const noexcept
    {
        return std::hash<std::string>{}(method.toString());
    }
};
