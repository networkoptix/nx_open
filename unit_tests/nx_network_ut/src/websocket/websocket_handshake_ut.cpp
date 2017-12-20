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

static void givenCorrectRequestHeaders(nx_http::Request* request)
{
    givenCorrectHeaders(request);

    auto& headers = request->headers;
    headers.emplace("Host", "www.hello.com");
    headers.emplace("Origin", "http://hello.com");
    headers.emplace("Sec-WebSocket-Key", kSecKey);
    headers.emplace("Sec-WebSocket-Protocol", "test");
    headers.emplace("Sec-WebSocket-Version", "13");
}

static void givenCorrectRequestLine(nx_http::Request* request)
{
    request->requestLine.method = "GET";
    request->requestLine.version = nx_http::http_1_1;
}

TEST(WebsocketHandshake, validateRequest_requestLine)
{
    nx_http::Request request;
    givenCorrectRequestHeaders(&request);
    givenCorrectRequestLine(&request);

    ASSERT_EQ(validateRequest(request, nullptr), Error::noError);

    request.requestLine.method = "POST";
    ASSERT_EQ(validateRequest(request, nullptr), Error::handshakeError);

    givenCorrectRequestLine(&request);
    request.requestLine.version = nx_http::http_1_0;
    ASSERT_EQ(validateRequest(request, nullptr), Error::handshakeError);
}

TEST(WebsocketHandshake, validateRequest_headers)
{
    nx_http::Request request;
    givenCorrectRequestLine(&request);

    givenCorrectRequestHeaders(&request);
    request.headers.erase("Origin");
    ASSERT_EQ(validateRequest(request, nullptr), Error::noError);

    givenCorrectRequestHeaders(&request);
    request.headers.erase("Sec-WebSocket-Protocol");
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
    nx_http::Request request;
    givenCorrectRequestLine(&request);
    givenCorrectRequestHeaders(&request);


    nx_http::Response response;
    ASSERT_EQ(validateRequest(request, &response), Error::noError);

    ASSERT_EQ(response.headers.find("Sec-WebSocket-Protocol")->second, "test");
    ASSERT_EQ(response.headers.find("Sec-WebSocket-Accept")->second,
        detail::makeAcceptKey(request.headers.find("Sec-WebSocket-Key")->second));
}

TEST(WebsocketHandshake, addClientHeaders)
{
    nx_http::Request request;
    addClientHeaders(&request.headers, "test");

    ASSERT_EQ(request.headers.find("Connection")->second, "Upgrade");
    ASSERT_EQ(request.headers.find("Upgrade")->second, "websocket");
    ASSERT_NE(request.headers.find("Sec-WebSocket-Key")->second.size(), 0);
    ASSERT_EQ(request.headers.find("Sec-WebSocket-Protocol")->second, "test");
    ASSERT_EQ(request.headers.find("Sec-WebSocket-Version")->second, "13");
}

TEST(WebsocketHandshake, validateResponse)
{
    nx_http::Request request;
    nx_http::Response response;

    givenCorrectRequestLine(&request);
    givenCorrectRequestHeaders(&request);

    ASSERT_EQ(validateRequest(request, &response), Error::noError);
    ASSERT_EQ(validateResponse(request, response), Error::noError);
}

}
