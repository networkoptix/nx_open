#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/random.h>
#include "websocket_handshake.h"

namespace nx {
namespace network {
namespace websocket {

const nx::Buffer kWebsocketProtocolName = "websocket";

namespace {

static const nx::Buffer kUpgrade = "Upgrade";
static const nx::Buffer kConnection = "Connection";
static const nx::Buffer kHost = "Host";
static const nx::Buffer kKey = "Sec-WebSocket-Key";
static const nx::Buffer kVersion = "Sec-WebSocket-Version";
static const nx::Buffer kProtocol = "Sec-WebSocket-Protocol";
static const nx::Buffer kAccept = "Sec-WebSocket-Accept";

static const nx::Buffer kVersionNum = "13";
static const nx::Buffer kMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";


static nx::Buffer makeClientKey()
{
    return nx::utils::random::generate(16).toBase64();
}

static Error validateRequestLine(const nx::network::http::RequestLine& requestLine)
{
    if (requestLine.method != "GET" || requestLine.version < nx::network::http::http_1_1)
        return Error::handshakeError;

    return Error::noError;
}

static Error validateHeaders(const nx::network::http::HttpHeaders& headers)
{
    nx::network::http::HttpHeaders::const_iterator headersIt;

    if (((headersIt = headers.find(kUpgrade)) == headers.cend() || headersIt->second != kWebsocketProtocolName)
        || ((headersIt = headers.find(kConnection)) == headers.cend() || headersIt->second != kUpgrade))
    {
        return Error::handshakeError;
    }

    auto websocketProtocolIt = headers.find(kProtocol);
    if (websocketProtocolIt != headers.cend() && websocketProtocolIt->second.isEmpty())
        return Error::handshakeError;

    return Error::noError;
}

static Error validateRequestHeaders(const nx::network::http::HttpHeaders& headers)
{
    if (validateHeaders(headers) != Error::noError)
        return Error::handshakeError;

    nx::network::http::HttpHeaders::const_iterator headersIt;

    if (headers.find(kHost) == headers.cend()
        || ((headersIt = headers.find(kKey)) == headers.cend() || headersIt->second.size() < 16)
        || ((headersIt = headers.find(kVersion)) == headers.cend() || headersIt->second != kVersionNum))
    {
        return Error::handshakeError;
    }

    return Error::noError;
}

static Error validateResponseHeaders(const nx::network::http::HttpHeaders& headers, const nx::network::http::Request& request)
{
    if (validateHeaders(headers) != Error::noError)
        return Error::handshakeError;

    nx::network::http::HttpHeaders::const_iterator headersIt;
    if ((headersIt = headers.find(kAccept)) == headers.cend()
        || detail::makeAcceptKey(request.headers.find(kKey)->second) != headersIt->second)
    {
        return Error::handshakeError;
    }

    auto requestProtocolIt = request.headers.find(kProtocol);
    auto responseProtocolIt = headers.find(kProtocol);

    if (requestProtocolIt != request.headers.cend())
    {
        if (responseProtocolIt == headers.cend())
            return Error::handshakeError;

        if (responseProtocolIt->second != requestProtocolIt->second)
            return Error::handshakeError;
    }

    return Error::noError;
}

} // namespace <anonymous>

namespace detail {

nx::Buffer makeAcceptKey(const nx::Buffer& requestKey)
{
    nx::Buffer result = nx::Buffer::fromBase64(requestKey);
    result += kMagic;
    nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Sha1);
    hash.addData(result);
    return hash.result().toBase64();
}

}

Error validateRequest(const nx::network::http::Request& request, nx::network::http::Response* response)
{
    Error result = validateRequestLine(request.requestLine);
    if (result != Error::noError)
        return result;

    result = validateRequestHeaders(request.headers);
    if (result != Error::noError)
        return result;

    if (!response)
        return Error::noError;

    response->statusLine.statusCode = nx::network::http::StatusCode::switchingProtocols;
    response->headers.emplace(kConnection, kUpgrade);
    response->headers.emplace(kUpgrade, kWebsocketProtocolName);
    response->headers.emplace(kAccept, detail::makeAcceptKey(request.headers.find(kKey)->second));

    auto websocketProtocolIt = request.headers.find(kProtocol);
    if (websocketProtocolIt != request.headers.cend())
        response->headers.emplace(kProtocol, websocketProtocolIt->second);

    return Error::noError;
}

void addClientHeaders(nx::network::http::HttpHeaders* headers, const nx::Buffer& protocolName)
{
    headers->emplace(kUpgrade, kWebsocketProtocolName);
    headers->emplace(kConnection, kUpgrade);
    headers->emplace(kKey, makeClientKey());
    headers->emplace(kVersion, kVersionNum);
    headers->emplace(kProtocol, protocolName);
}

void addClientHeaders(nx::network::http::AsyncClient* request, const nx::Buffer& protocolName)
{
    nx::network::http::HttpHeaders headers;
    addClientHeaders(&headers, protocolName);
    request->setAdditionalHeaders(std::move(headers));
}

Error validateResponse(const nx::network::http::Request& request, const nx::network::http::Response& response)
{
    if (response.statusLine.statusCode != nx::network::http::StatusCode::switchingProtocols)
        return Error::handshakeError;

    if (validateResponseHeaders(response.headers, request) != Error::noError)
        return Error::handshakeError;

    return Error::noError;
}

} // namespace websocket
} // namespace network
} // namespace nx
