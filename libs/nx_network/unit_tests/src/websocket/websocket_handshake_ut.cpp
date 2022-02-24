// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/websocket/websocket_handshake.h>

namespace test {

using namespace nx::network::websocket;

const nx::Buffer kSecKey = "1234567898765432";

template<typename Message>
static void givenCorrectHeaders(Message* message)
{
    auto& headers = message->headers;
    headers.clear();
    headers.emplace("Connection", "Upgrade");
    headers.emplace("Upgrade", "websocket");
}

static void givenCorrectRequestHeaders(
    nx::network::http::Request* request, CompressionType compressionType = CompressionType::none)
{
    givenCorrectHeaders(request);

    auto& headers = request->headers;
    headers.emplace("Host", "www.hello.com");
    headers.emplace("Origin", "http://hello.com");
    headers.emplace("Sec-WebSocket-Key", kSecKey);
    headers.emplace("Sec-WebSocket-Protocol", "test");
    headers.emplace("Sec-WebSocket-Version", "13");

    if (compressionType == CompressionType::perMessageDeflate)
        headers.emplace("Sec-WebSocket-Extensions", "permessage-deflate");
}

static void givenCorrectRequestLine(nx::network::http::Request* request)
{
    request->requestLine.method = "GET";
    request->requestLine.version = nx::network::http::http_1_1;
}

TEST(WebsocketHandshake, validateRequest_requestLine)
{
    nx::network::http::Request request;
    givenCorrectRequestHeaders(&request);
    givenCorrectRequestLine(&request);

    ASSERT_EQ(validateRequest(request, nullptr), Error::noError);

    request.requestLine.method = "POST";
    ASSERT_EQ(validateRequest(request, nullptr), Error::handshakeError);

    givenCorrectRequestLine(&request);
    request.requestLine.version = nx::network::http::http_1_0;
    ASSERT_EQ(validateRequest(request, nullptr), Error::handshakeError);
}

TEST(WebsocketHandshake, validateRequest_headers)
{
    using namespace nx::network::http;

    Request request;
    givenCorrectRequestLine(&request);

    givenCorrectRequestHeaders(&request);
    request.headers.erase("Origin");
    ASSERT_EQ(validateRequest(request, nullptr), Error::noError);

    givenCorrectRequestHeaders(&request);
    request.headers.erase("Sec-WebSocket-Protocol");
    ASSERT_EQ(validateRequest(request, nullptr), Error::noError);

    givenCorrectRequestHeaders(&request);
    insertOrReplaceHeader(&request.headers, HttpHeader("Connection", "upgrade"));
    ASSERT_EQ(validateRequest(request, nullptr), Error::noError);

    givenCorrectRequestHeaders(&request);
    request.headers.erase("Host");
    ASSERT_EQ(validateRequest(request, nullptr), Error::handshakeError);

    givenCorrectRequestHeaders(&request);
    request.headers.erase("Sec-WebSocket-Key");
    ASSERT_EQ(validateRequest(request, nullptr), Error::handshakeError);

    givenCorrectRequestHeaders(&request);
    request.headers.erase("Sec-WebSocket-Version");
    ASSERT_EQ(validateRequest(request, nullptr), Error::handshakeError);

    givenCorrectRequestHeaders(&request);
    request.headers.find("Sec-WebSocket-Version")->second = "78";
    ASSERT_EQ(validateRequest(request, nullptr), Error::handshakeError);
}

TEST(WebsocketHandshake, validateRequest_response)
{
    nx::network::http::Request request;
    givenCorrectRequestLine(&request);
    givenCorrectRequestHeaders(&request);

    nx::network::http::Response response;
    ASSERT_EQ(validateRequest(request, &response), Error::noError);

    ASSERT_EQ(response.headers.find("Sec-WebSocket-Protocol")->second, "test");
    ASSERT_EQ(response.headers.find("Sec-WebSocket-Accept")->second,
        detail::makeAcceptKey(request.headers.find("Sec-WebSocket-Key")->second));

    ASSERT_EQ(response.headers.find("Sec-WebSocket-Extensions"), response.headers.cend());
}

TEST(WebsocketHandshake, validateRequest_response_Compression)
{
    nx::network::http::Request request;
    givenCorrectRequestLine(&request);
    givenCorrectRequestHeaders(&request, CompressionType::perMessageDeflate);
    nx::network::http::Response response;
    ASSERT_EQ(validateRequest(request, &response), Error::noError);

    ASSERT_EQ(response.headers.find("Sec-WebSocket-Protocol")->second, "test");
    ASSERT_EQ(response.headers.find("Sec-WebSocket-Accept")->second,
        detail::makeAcceptKey(request.headers.find("Sec-WebSocket-Key")->second));

    ASSERT_EQ(response.headers.find("Sec-WebSocket-Extensions")->second, "permessage-deflate");
}

TEST(WebsocketHandshake, addClientHeaders)
{
    nx::network::http::Request request;
    addClientHeaders(&request.headers, "test", CompressionType::none);

    ASSERT_EQ(request.headers.find("Connection")->second, "Upgrade");
    ASSERT_EQ(request.headers.find("Upgrade")->second, "websocket");
    ASSERT_NE(request.headers.find("Sec-WebSocket-Key")->second.size(), 0);
    ASSERT_EQ(request.headers.find("Sec-WebSocket-Protocol")->second, "test");
    ASSERT_EQ(request.headers.find("Sec-WebSocket-Version")->second, "13");

    ASSERT_EQ(request.headers.find("Sec-WebSocket-Extensions"), request.headers.cend());
}

TEST(WebsocketHandshake, addClientHeaders_Compression)
{
    nx::network::http::Request request;
    addClientHeaders(&request.headers, "test", CompressionType::perMessageDeflate);

    ASSERT_EQ(request.headers.find("Sec-WebSocket-Extensions")->second, "permessage-deflate");
}


TEST(WebsocketHandshake, validateResponse)
{
    nx::network::http::Request request;
    nx::network::http::Response response;

    givenCorrectRequestLine(&request);
    givenCorrectRequestHeaders(&request);

    ASSERT_EQ(validateRequest(request, &response), Error::noError);
    ASSERT_EQ(validateResponse(request, response), Error::noError);
}

} // namespace test
