// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_handshake.h"

#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/random.h>
#include <nx/utils/std/algorithm.h>

namespace nx::network::websocket {

namespace {

static const std::string kUpgrade = "Upgrade";
static const std::string kConnection = "Connection";
static const std::string kHost = "Host";
static const std::string kKey = "Sec-WebSocket-Key";
static const std::string kVersion = "Sec-WebSocket-Version";
static const std::string kProtocol = "Sec-WebSocket-Protocol";
static const std::string kAccept = "Sec-WebSocket-Accept";
static const std::string kExtension = "Sec-WebSocket-Extensions";
static const std::string kCompressionAllowed = "permessage-deflate";

static const std::string kVersionNum = "13";
static const std::string kMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static const nx::log::Tag kWebsocketTag{QString("websocket")};

static nx::Buffer makeClientKey()
{
    return nx::Buffer(nx::utils::random::generate(16).toBase64());
}

static Error validateRequestLine(const nx::network::http::RequestLine& requestLine)
{
    if ((requestLine.method != nx::network::http::Method::get
            && requestLine.method != nx::network::http::Method::options)
        || requestLine.version < nx::network::http::http_1_1)
    {
        return Error::handshakeError;
    }

    return Error::noError;
}

static Error validateHeaders(const nx::network::http::HttpHeaders& headers)
{
    nx::network::http::HttpHeaders::const_iterator headersIt;

    if (((headersIt = headers.find(kUpgrade)) == headers.cend()
            || nx::utils::stricmp(headersIt->second, kWebsocketProtocolName) != 0)
        || ((headersIt = headers.find(kConnection)) == headers.cend()
            || !nx::utils::contains(nx::utils::toLower(headersIt->second), nx::utils::toLower(kUpgrade))))
            // ||!nx::utils::contains(headersIt->second, kUpgrade, nx::utils::ci_equal())
    {
        return Error::handshakeError;
    }

    auto websocketProtocolIt = headers.find(kProtocol);
    if (websocketProtocolIt != headers.cend() && websocketProtocolIt->second.empty())
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

static Error validateResponseHeaders(
    const nx::network::http::HttpHeaders& headers,
    const nx::network::http::Request& request)
{
    if (validateHeaders(headers) != Error::noError)
    {
        NX_DEBUG(kWebsocketTag, "Validation failed because header validation failed:", headers);
        return Error::handshakeError;
    }

    nx::network::http::HttpHeaders::const_iterator headersIt;
    if ((headersIt = headers.find(kAccept)) == headers.cend()
        || detail::makeAcceptKey(request.headers.find(kKey)->second) != headersIt->second)
    {
        NX_DEBUG(kWebsocketTag, "Validation failed because accept key processing failed:", headers);
        return Error::handshakeError;
    }

    auto requestProtocolIt = request.headers.find(kProtocol);
    auto responseProtocolIt = headers.find(kProtocol);

    if (requestProtocolIt != request.headers.cend())
    {
        if (responseProtocolIt == headers.cend())
        {
            NX_DEBUG(
                kWebsocketTag,
                "Validation failed because websocket protocol header has not been found:",
                headers);
            return Error::handshakeError;
        }

        if (responseProtocolIt->second != requestProtocolIt->second)
        {
            NX_DEBUG(
                kWebsocketTag,
                "Validation failed because of websocket protocol mismatch:", headers);
            return Error::handshakeError;
        }
    }

    return Error::noError;
}

} // namespace <anonymous>

namespace detail {

nx::Buffer makeAcceptKey(const nx::ConstBufferRefType& requestKey)
{
    nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Sha1);
    hash.addData(requestKey.data(), requestKey.size());
    hash.addData(kMagic.data(), kMagic.size());
    return nx::Buffer(hash.result().toBase64());
}

} // namespace detail

CompressionType compressionType(const nx::network::http::HttpHeaders& headers)
{
    auto extensionHeaderItr = headers.find(kExtension);
    if (extensionHeaderItr != headers.cend()
        && nx::utils::contains(extensionHeaderItr->second, kCompressionAllowed))
    {
        return CompressionType::perMessageDeflate;
    }
    return CompressionType::none;
}

Error validateRequest(
    const nx::network::http::Request& request,
    nx::network::http::Response* response,
    bool disableCompression)
{
    const auto res = validateRequest(request, response ? &response->headers : nullptr, disableCompression);
    if (response && res == Error::noError)
        response->statusLine.statusCode = nx::network::http::StatusCode::switchingProtocols;
    return res;
}

Error validateRequest(
    const nx::network::http::Request& request,
    nx::network::http::HttpHeaders* responseHeaders,
    bool disableCompression)
{
    Error result = validateRequestLine(request.requestLine);
    if (result != Error::noError)
        return result;

    result = validateRequestHeaders(request.headers);
    if (result != Error::noError)
        return result;

    if (!responseHeaders)
        return Error::noError;

    responseHeaders->emplace(kConnection, kUpgrade);
    responseHeaders->emplace(kUpgrade, kWebsocketProtocolName);
    responseHeaders->emplace(kAccept, detail::makeAcceptKey(request.headers.find(kKey)->second));

    auto websocketProtocolIt = request.headers.find(kProtocol);
    if (websocketProtocolIt != request.headers.cend())
        responseHeaders->emplace(kProtocol, websocketProtocolIt->second);

    if (compressionType(request.headers) != CompressionType::none && !disableCompression)
        responseHeaders->emplace(kExtension, kCompressionAllowed);

    return Error::noError;
}

void addClientHeaders(
    nx::network::http::HttpHeaders* headers,
    const nx::Buffer& protocolName,
    CompressionType compressionType)
{
    headers->emplace(kUpgrade, kWebsocketProtocolName);
    headers->emplace(kConnection, kUpgrade);
    headers->emplace(kKey, makeClientKey());
    headers->emplace(kVersion, kVersionNum);
    headers->emplace(kProtocol, protocolName);
    if (compressionType != CompressionType::none)
        headers->emplace(kExtension, kCompressionAllowed);
}

void addClientHeaders(
    nx::network::http::AsyncClient* request,
    const nx::Buffer& protocolName,
    CompressionType compressionType)
{
    nx::network::http::HttpHeaders headers;
    addClientHeaders(&headers, protocolName, compressionType);
    request->addRequestHeaders(std::move(headers));
}

void addClientHeaders(
    nx::network::http::HttpClient* client,
    const nx::Buffer& protocolName,
    CompressionType compressionType)
{
    nx::network::http::HttpHeaders headers;
    addClientHeaders(&headers, protocolName, compressionType);
    for (const auto& [k, v]: headers)
        client->addAdditionalHeader(k, v);
}

void copyClientHeaders(
    nx::network::http::HttpHeaders* dstHeaders,
    const nx::network::http::HttpHeaders* srcHeaders)
{
    for (const auto& srcHeader: *srcHeaders)
    {
        if (nx::utils::stricmp(srcHeader.first, kUpgrade) == 0
            || nx::utils::stricmp(srcHeader.first, kConnection) == 0
            || nx::utils::stricmp(srcHeader.first, kKey) == 0
            || nx::utils::stricmp(srcHeader.first, kVersion) == 0
            || nx::utils::stricmp(srcHeader.first, kProtocol) == 0
            || nx::utils::stricmp(srcHeader.first, kAccept) == 0
            || nx::utils::stricmp(srcHeader.first, kExtension) == 0)
        {
            http::insertOrReplaceHeader(dstHeaders, srcHeader);
        }
    }
}

Error validateResponse(
    const nx::network::http::Request& request,
    const nx::network::http::Response& response)
{
    if (response.statusLine.statusCode != nx::network::http::StatusCode::switchingProtocols)
    {
        NX_DEBUG(
            kWebsocketTag, "Validation failed because response code is not 'switchingProtocols'");
        return Error::handshakeError;
    }

    if (validateResponseHeaders(response.headers, request) != Error::noError)
        return Error::handshakeError;

    return Error::noError;
}

} // namespace nx::network::websocket
