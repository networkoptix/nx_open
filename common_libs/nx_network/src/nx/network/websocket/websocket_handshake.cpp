#include <utils/common/cryptographic_hash.h>
#include <nx/utils/random.h>
#include "websocket_handshake.h"

namespace nx {
namespace network {
namespace websocket {

namespace {

static const nx::Buffer kUpgrade = "Upgrade";
static const nx::Buffer kConnection = "Connection";
static const nx::Buffer kHost = "Host";
static const nx::Buffer kKey = "Sec-WebSocket-Key";
static const nx::Buffer kVersion = "Sec-WebSocket-Version";
static const nx::Buffer kProtocol = "Sec-WebSocket-Protocol";
static const nx::Buffer kAccept = "Sec-WebSocket-Accept";

static const nx::Buffer kWebsocket = "websocket";
static const nx::Buffer kVersionNum = "13";
static const nx::Buffer kMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";


static nx::Buffer makeAcceptKey(const nx::Buffer& requestKey)
{
    nx::Buffer result = nx::Buffer::fromBase64(requestKey);
    result += kMagic;
    QnCryptographicHash hash(QnCryptographicHash::Sha1);
    hash.addData(result);
    return hash.result().toBase64();
}

static nx::Buffer makeClientKey()
{
    return nx::utils::random::generate(16).toBase64();
}

static Error validateRequestLine(const nx_http::RequestLine& requestLine)
{
    if (requestLine.method != "GET" || requestLine.version < nx_http::http_1_1)
        return Error::handshakeError;

    return Error::noError;
}

static Error validateHeaders(const nx_http::HttpHeaders& headers)
{
    nx_http::HttpHeaders::const_iterator headersIt;

    if (((headersIt = headers.find(kUpgrade)) == headers.cend() || headersIt->second != kWebsocket)
        || ((headersIt = headers.find(kConnection)) == headers.cend() || headersIt->second != kUpgrade))  
    {
        return Error::handshakeError;
    }

    auto websocketProtocolIt = headers.find(kProtocol);
    if (websocketProtocolIt != headers.cend() && websocketProtocolIt->second.isEmpty())
        return Error::handshakeError;

    return Error::noError;
}

static Error validateRequestHeaders(const nx_http::HttpHeaders& headers)
{
    if (validateHeaders(headers) != Error::noError)
        return Error::handshakeError;

    nx_http::HttpHeaders::const_iterator headersIt;

    if (headers.find(kHost) == headers.cend()
        || ((headersIt = headers.find(kKey)) == headers.cend() || headersIt->second.size() < 16)
        || ((headersIt = headers.find(kVersion)) == headers.cend() || headersIt->second != kVersionNum))
    {
        return Error::handshakeError;
    }

    return Error::noError;
}

static Error validateResponseHeaders(const nx_http::HttpHeaders& headers, const nx::Buffer& requestKey)
{
    if (validateHeaders(headers) != Error::noError)
        return Error::handshakeError;

    nx_http::HttpHeaders::const_iterator headersIt;
    if ((headersIt = headers.find(kAccept)) == headers.cend() || makeAcceptKey(requestKey) != headersIt->second)
        return Error::handshakeError;

    return Error::noError;
}


} // namespace <anonymous>

Error validateRequest(const nx_http::Request& request, nx_http::Response* response)
{
    Error result = validateRequestLine(request.requestLine);
    if (result != Error::noError)
        return result;

    result = validateRequestHeaders(request.headers);
    if (result != Error::noError)
        return result;

    response->statusLine.statusCode = nx_http::StatusCode::upgrade;
    response->headers.emplace(kConnection, kUpgrade);
    response->headers.emplace(kUpgrade, kWebsocket);
    response->headers.emplace(kAccept, makeAcceptKey(request.headers.find(kKey)->second));

    auto websocketProtocolIt = request.headers.find(kProtocol);
    if (websocketProtocolIt != request.headers.cend())
        response->headers.emplace(kProtocol, websocketProtocolIt->second);

    return Error::noError;
}

void addClientHeaders(nx_http::Request* request, const nx::Buffer& protocolName)
{
    request->headers.emplace(kUpgrade, kWebsocket);
    request->headers.emplace(kConnection, kUpgrade);
    request->headers.emplace(kKey, makeClientKey());
    request->headers.emplace(kVersion, kVersionNum);
    request->headers.emplace(kProtocol, protocolName);
}

Error validateResponse(const nx_http::Request& request, const nx_http::Response& response)
{
    if (response.statusLine.statusCode != nx_http::StatusCode::upgrade)
        return Error::handshakeError;

    if (validateResponseHeaders(response.headers, request.headers.find(kKey)->second) != Error::noError)
        return Error::handshakeError;

    return Error::noError;
}

} // namespace websocket
} // namespace network
} // namespace nx
