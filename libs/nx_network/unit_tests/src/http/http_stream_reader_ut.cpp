// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <optional>

#include <gtest/gtest.h>

#include <nx/network/http/http_stream_reader.h>
#include <nx/utils/move_only_func.h>

namespace nx::network::http::test {

// TODO: #akolesnikov Refactor tests here.

class TestMessageData
{
public:
    nx::Buffer headers;
    nx::Buffer msgBody;
    bool chunked = false;

    TestMessageData(
        nx::Buffer headers,
        nx::Buffer msgBody)
        :
        headers(std::move(headers)),
        msgBody(std::move(msgBody))
    {
        chunked = headers.find("Transfer-Encoding: chunked") != headers.npos;
    }
};

class TestRequestData:
    public TestMessageData
{
public:
    Method method;

    TestRequestData(
        const std::string_view& method,
        nx::Buffer headers,
        nx::Buffer msgBody)
        :
        TestMessageData(std::move(headers), std::move(msgBody)),
        method(method)
    {
    }
};

//-------------------------------------------------------------------------------------------------

class TestResponseData:
    public TestMessageData
{
public:
    int statusCode;

    TestResponseData(
        int statusCode,
        nx::Buffer headers,
        nx::Buffer msgBody)
        :
        TestMessageData(std::move(headers), std::move(msgBody)),
        statusCode(statusCode)
    {
    }
};

//-------------------------------------------------------------------------------------------------

class HttpStreamReader:
    public ::testing::Test
{
protected:
    struct RequestDescriptor
    {
        const Method method;
        std::optional<nx::Buffer> body = std::nullopt;
    };

    struct ResponseDescriptor
    {
        const StatusCode::Value status;
        std::optional<nx::Buffer> body = std::nullopt;
    };

    void addRequestToInputStream(RequestDescriptor request)
    {
        m_inputMessages.push_back(prepareMessage(std::move(request)));
        m_inputStream += m_inputMessages.back().toString();
    }

    void addResponseToInputStream(ResponseDescriptor response)
    {
        m_inputMessages.push_back(prepareMessage(std::move(response)));
        m_inputStream += m_inputMessages.back().toString();
    }

    void parseInputStream()
    {
        ConstBufferRefType buf(m_inputStream);

        m_parseResult = false;

        for (int i = 0; !buf.empty() && i < 10000; ++i) //< Protecting from a dead loop.
        {
            std::size_t bytesParsed = 0;
            if (!m_parser.parseBytes(buf, &bytesParsed))
                return;

            buf.remove_prefix(bytesParsed);

            switch (m_parser.state())
            {
                case http::HttpStreamReader::ReadState::messageDone:
                {
                    auto msg = m_parser.takeMessage();
                    msg.setBody(m_parser.fetchMessageBody());
                    m_outputMessages.push_back(std::move(msg));
                    break;
                }

                case http::HttpStreamReader::ReadState::parseError:
                    return;

                default:
                    break;
            }
        }

        m_parseResult = true;
    }

    /**
     * Parses buf using reader reading chunks of size readStep from the buf.
     * @param func Invoked on every message parsed.
     */
    void parseBufWithFixedReadStep(
        const nx::Buffer& buf,
        std::size_t readStep,
        nx::utils::MoveOnlyFunc<void(http::HttpStreamReader&, const Message&)> func,
        bool ignoreParseErrors = false)
    {
        http::HttpStreamReader reader;

        for (std::size_t i = 0; i < buf.size();)
        {
            size_t bytesProcessed = 0;
            const auto parseResult = reader.parseBytes(
                nx::ConstBufferRefType(buf.data() + i, std::min<int>(readStep, buf.size() - i)),
                &bytesProcessed);

            if (!ignoreParseErrors)
            {
                ASSERT_TRUE(parseResult);
                ASSERT_NE(http::HttpStreamReader::ReadState::parseError, reader.state());
            }

            i += bytesProcessed;

            if (reader.state() == http::HttpStreamReader::ReadState::messageDone)
                func(reader, reader.message());
        }
    }

    void assertParseSucceeds()
    {
        ASSERT_TRUE(m_parseResult);
    }

    void assertParsedMessageStreamMathesInput()
    {
        ASSERT_EQ(m_inputMessages, m_outputMessages);
    }

    nx::Buffer serializeMessages(const std::vector<TestRequestData>& messages)
    {
        nx::Buffer buf;

        int requestNumber = 0;
        for (const auto& msgData: messages)
        {
            buf += msgData.method.toString();
            buf += " / HTTP/1.1\r\n";
            buf += msgData.headers;
            buf += "Test-Request-Number: " + std::to_string(requestNumber++) + "\r\n";
            if (!msgData.msgBody.empty() && !msgData.chunked)
                buf += "Content-Length: " + std::to_string(msgData.msgBody.size()) + "\r\n";
            buf += "\r\n";
            buf += msgData.msgBody;
        }

        return buf;
    }

    nx::Buffer serializeMessages(const std::vector<TestResponseData>& messages)
    {
        nx::Buffer buf;

        int requestNumber = 0;
        for (const auto& msgData: messages)
        {
            buf += "HTTP/1.1 " + std::to_string(msgData.statusCode) + " " + StatusCode::toString(msgData.statusCode) + "\r\n";
            buf += msgData.headers;
            buf += "Test-Request-Number: " + std::to_string(requestNumber++) + "\r\n";
            if (!msgData.msgBody.empty() && !msgData.chunked)
                buf += "Content-Length: " + std::to_string(msgData.msgBody.size()) + "\r\n";
            buf += "\r\n";
            buf += msgData.msgBody;
        }

        return buf;
    }

private:
    Message prepareMessage(RequestDescriptor desc)
    {
        Message msg(MessageType::request);
        msg.request->requestLine.method = desc.method;
        msg.request->requestLine.url = "/test";
        msg.request->requestLine.version = http_1_1;
        if (desc.body)
            addBody(&msg, std::move(*desc.body));

        return msg;
    }

    Message prepareMessage(ResponseDescriptor desc)
    {
        Message msg(MessageType::response);
        msg.response->statusLine.version = http_1_1;
        msg.response->statusLine.statusCode = desc.status;
        msg.response->statusLine.reasonPhrase = StatusCode::toString(desc.status);
        if (desc.body)
            addBody(&msg, std::move(*desc.body));

        return msg;
    }

    void addBody(Message* msg, nx::Buffer body)
    {
        msg->headers().emplace(header::kContentType, "text/plain");
        msg->headers().emplace(header::kContentLength, std::to_string(body.size()));
        msg->setBody(std::move(body));
    }

private:
    http::HttpStreamReader m_parser;
    std::vector<Message> m_inputMessages;
    nx::Buffer m_inputStream;
    bool m_parseResult = false;
    std::vector<Message> m_outputMessages;
};

TEST_F(HttpStreamReader, parsingRequest)
{
    const char requestStr[] =
        "GET /hhh HTTP/1.1\r\n"
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"example.com\",nonce=\"50df1b6e2a378\"\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        //"Content-Length: 0\r\n"
        "\r\n";

    nx::Buffer sourceDataStream(requestStr);

    for (std::size_t dataStep = 1; dataStep < 16 * 1024; dataStep <<= 1)
    {
        http::HttpStreamReader streamReader;
        for (std::size_t pos = 0; pos < sourceDataStream.size(); )
        {
            size_t bytesProcessed = 0;
            ASSERT_TRUE(streamReader.parseBytes(
                nx::ConstBufferRefType(
                    sourceDataStream.data() + pos,
                    std::min(dataStep, sourceDataStream.size() - pos)),
                &bytesProcessed));
            //ASSERT_TRUE( bytesProcessed > 0 );
            pos += bytesProcessed;
            ASSERT_TRUE(bytesProcessed != 0);
        }

        //TODO #akolesnikov validate parsed request
        ASSERT_EQ(streamReader.state(), http::HttpStreamReader::ReadState::messageDone);
        ASSERT_EQ(streamReader.message().type, nx::network::http::MessageType::request);
        ASSERT_TRUE(streamReader.message().request->messageBody.empty());
    }
}

TEST_F(HttpStreamReader, parsingInvalidHeaderResponse)
{
    const char invalidHeaderResponse[] =
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "invalid header\r\n"
        "Content-Type: application/text\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "test";

    const auto invalidHeaderData =
        QByteArray::fromRawData(invalidHeaderResponse, sizeof(invalidHeaderResponse) - 1);

    nx::network::http::HttpStreamReader streamReader;
    size_t bytesProcessed = 0;

    ASSERT_FALSE(streamReader.parseBytes(invalidHeaderData, &bytesProcessed));
    ASSERT_EQ(streamReader.state(), http::HttpStreamReader::ReadState::parseError);

    nx::network::http::HttpStreamReader tolerateStreamReader;
    bytesProcessed = 0;

    tolerateStreamReader.setParseHeadersStrict(false);

    ASSERT_TRUE(tolerateStreamReader.parseBytes(invalidHeaderData, &bytesProcessed));
    ASSERT_EQ(bytesProcessed, invalidHeaderData.size());
    ASSERT_EQ(tolerateStreamReader.state(), http::HttpStreamReader::ReadState::messageDone);
}

TEST_F(HttpStreamReader, MultipleMessages)
{
    //vector<pair<message, message body>>
    std::vector<TestResponseData> messagesToParse;

    messagesToParse.push_back(TestResponseData(
        200,

        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",

        nx::utils::buildString<nx::Buffer>("1", std::string(400, 'x'), "2")
    ));

    messagesToParse.push_back(TestResponseData(
        200,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",

        nx::utils::buildString<nx::Buffer>("3", std::string(200, 'h'), "4")
    ));

    messagesToParse.push_back(TestResponseData(
        451,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n"
        "Content-Length: 0\r\n",

        ""
    ));

    messagesToParse.push_back(TestResponseData(
        200,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",

        nx::utils::buildString<nx::Buffer>("5", std::string(120, 'h'), "6")
    ));


    messagesToParse.push_back(TestResponseData(
        200,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "Content-Length: 0\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        ""
    ));

    messagesToParse.push_back(TestResponseData(
        200,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "Transfer-Encoding: chunked\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        "0000020\r\n"
        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        "\r\n"
        "0000040\r\n"
        "3xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx4"
        "5xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6"
        "\r\n"
        "0000060\r\n"
        "7xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx8"
        "9xxxxxxxxxxxxxxxxxxxxxxxxxxxxx10"
        "11xxxxxxxxxxxxxxxxxxxxxxxxxxxx12"
        "\r\n"
        "0\r\n"
        "\r\n"
    ));

    messagesToParse.push_back(TestResponseData(
        401,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"example.com\",nonce=\"50df1b6e2a378\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        nx::utils::buildString<nx::Buffer>("1", std::string(2500, 'x'), "2")
    ));

    messagesToParse.push_back(TestResponseData(
        401,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"example.com\",nonce=\"50df1b6e2b700\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        nx::utils::buildString<nx::Buffer>("1", std::string(2500, 'x'), "2")
    ));

    messagesToParse.push_back(TestResponseData(
        401,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"example.com\",nonce=\"50df1b6e2ca88\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        nx::utils::buildString<nx::Buffer>("1", std::string(2500, 'x'), "2")
    ));

    messagesToParse.push_back(TestResponseData(
        200,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "Content-Length: 0\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        ""));

    messagesToParse.push_back(TestResponseData(
        200,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "Transfer-Encoding: chunked\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",


        "0000020;time_sync_6248_1422706690167_17640391744334\r\n"
        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        "\r\n"
        "0000040;time_sync_6248_1422706690167_17640391744334\r\n"
        "3xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx4"
        "5xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6"
        "\r\n"
        "0000060;time_sync_6248_1422706690167_17640391744334\r\n"
        "7xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx8"
        "9xxxxxxxxxxxxxxxxxxxxxxxxxxxxx10"
        "11xxxxxxxxxxxxxxxxxxxxxxxxxxxx12"
        "\r\n"
        "0\r\n"
        "\r\n"
    ));

    nx::Buffer sourceDataStream = serializeMessages(messagesToParse);

    for (std::size_t dataStep = 1; dataStep < 16 * 1024; dataStep <<= 1)
    {
        int messageNumber = 0;

        parseBufWithFixedReadStep(
            sourceDataStream, dataStep,
            [&messageNumber, &messagesToParse](auto& reader, const auto& msg)
            {
                ASSERT_EQ(messageNumber, reader.currentMessageNumber());
                ASSERT_EQ(nx::network::http::MessageType::response, msg.type);
                ASSERT_NE(nullptr, msg.response);
                ASSERT_EQ("HTTP", msg.response->statusLine.version.protocol);
                ASSERT_EQ(messagesToParse[messageNumber].statusCode, msg.response->statusLine.statusCode);

                const auto msgBody = reader.fetchMessageBody();
                if (!messagesToParse[messageNumber].chunked)
                {
                    ASSERT_EQ(
                        messagesToParse[messageNumber].msgBody.size(),
                        nx::utils::stoi(getHeaderValue(msg.response->headers, "Content-Length")));
                    ASSERT_TRUE(msgBody == messagesToParse[messageNumber].msgBody);
                }
                else
                {
                    // TODO #akolesnikov check chunked message body
                }

                ++messageNumber;
            });

        ASSERT_EQ((size_t) messageNumber, messagesToParse.size());
    }
}

TEST_F(HttpStreamReader, MultipleRequests)
{
    //vector<pair<message, message body>>
    std::vector<TestRequestData> messagesToParse;

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",

        nx::utils::buildString<nx::Buffer>("1", std::string(420, 'x'), "2")
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n"
        "Content-Length: 0\r\n",

        ""
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",

        nx::utils::buildString<nx::Buffer>("3", std::string(200, 'h'), "4")
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n"
        "Content-Length: 0\r\n",

        ""
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",

        nx::utils::buildString<nx::Buffer>("5", std::string(120, 'h'), "6")
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "Content-Length: 0\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        ""
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "Transfer-Encoding: chunked\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        "0000020\r\n"
        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        "\r\n"
        "0000040\r\n"
        "3xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx4"
        "5xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6"
        "\r\n"
        "0000060\r\n"
        "7xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx8"
        "9xxxxxxxxxxxxxxxxxxxxxxxxxxxxx10"
        "11xxxxxxxxxxxxxxxxxxxxxxxxxxxx12"
        "\r\n"
        "0\r\n"
        "\r\n"
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"example.com\",nonce=\"50df1b6e2a378\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        nx::utils::buildString<nx::Buffer>("1", std::string(2500, 'x'), "2")
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"example.com\",nonce=\"50df1b6e2b700\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        nx::utils::buildString<nx::Buffer>("1", std::string(2500, 'x'), "2")
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"example.com\",nonce=\"50df1b6e2ca88\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        nx::utils::buildString<nx::Buffer>("1", std::string(2500, 'x'), "2")
    ));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "Content-Length: 0\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        ""));

    messagesToParse.push_back(TestRequestData(
        nx::network::http::Method::post,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "Transfer-Encoding: chunked\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",


        "0000020;time_sync_6248_1422706690167_17640391744334\r\n"
        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        "\r\n"
        "0000040;time_sync_6248_1422706690167_17640391744334\r\n"
        "3xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx4"
        "5xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6"
        "\r\n"
        "0000060;time_sync_6248_1422706690167_17640391744334\r\n"
        "7xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx8"
        "9xxxxxxxxxxxxxxxxxxxxxxxxxxxxx10"
        "11xxxxxxxxxxxxxxxxxxxxxxxxxxxx12"
        "\r\n"
        "0\r\n"
        "\r\n"
    ));

    nx::Buffer sourceDataStream = serializeMessages(messagesToParse);

    for (std::size_t dataStep = 1; dataStep < (sourceDataStream.size() * 2); dataStep <<= 1)
        //for( int dataStep = 1; dataStep < sourceDataStream.size(); ++dataStep )
    {
        int messageNumber = 0;

        parseBufWithFixedReadStep(
            sourceDataStream, dataStep,
            [&messageNumber, &messagesToParse](auto& reader, const auto& msg)
            {
                ASSERT_EQ(MessageType::request, reader.message().type);
                ASSERT_NE(nullptr, msg.request);
                ASSERT_EQ(msg.request->requestLine.method, messagesToParse[messageNumber].method);

                const auto msgBody = reader.fetchMessageBody();
                if (!messagesToParse[messageNumber].chunked)
                {
                    ASSERT_EQ(
                        messagesToParse[messageNumber].msgBody.size(),
                        nx::utils::stoi(nx::network::http::getHeaderValue(msg.request->headers, "Content-Length")));
                    ASSERT_TRUE(msgBody == messagesToParse[messageNumber].msgBody);
                }
                else
                {
                    // TODO #akolesnikov Check chunked message body.
                }

                ++messageNumber;
            });

        ASSERT_EQ((size_t)messageNumber, messagesToParse.size());
    }
}

TEST_F(
    HttpStreamReader,
    moves_to_reading_message_body_state_after_reading_full_headers_body_separator)
{
    static const nx::Buffer kMessage =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n";

    http::HttpStreamReader reader;

    std::size_t bytesParsed = 0;
    ASSERT_TRUE(reader.parseBytes(kMessage, &bytesParsed));

    ASSERT_EQ(kMessage.size(), bytesParsed);
    ASSERT_EQ(http::HttpStreamReader::ReadState::readingMessageBody, reader.state());
}

TEST_F(
    HttpStreamReader,
    moves_to_intermediate_state_after_reading_partial_headers_body_separator)
{
    static const nx::Buffer kMessage =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "\r";

    http::HttpStreamReader reader;

    std::size_t bytesParsed = 0;
    ASSERT_TRUE(reader.parseBytes(kMessage, &bytesParsed));

    ASSERT_EQ(kMessage.size(), bytesParsed);
    ASSERT_EQ(http::HttpStreamReader::ReadState::pullingLineEndingBeforeMessageBody, reader.state());

    ASSERT_TRUE(reader.parseBytes(nx::Buffer("\n"), &bytesParsed));
    ASSERT_EQ(1, bytesParsed);
    ASSERT_EQ(http::HttpStreamReader::ReadState::readingMessageBody, reader.state());
}

TEST_F(HttpStreamReader, interrupts_before_parsing_message_body)
{
    static const nx::Buffer kHeaders =
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n";

    static const nx::Buffer kMessageBody =
        "The message body";

    static const nx::Buffer kMessage =
        kHeaders +
        kMessageBody;

    http::HttpStreamReader reader;
    reader.setBreakAfterReadingHeaders(true);
    std::size_t bytesParsed = 0;

    nx::ConstBufferRefType dataToParse(kMessage);
    ASSERT_TRUE(reader.parseBytes(dataToParse, &bytesParsed));
    ASSERT_EQ(http::HttpStreamReader::ReadState::readingMessageBody, reader.state());
    ASSERT_EQ(kHeaders.size(), bytesParsed);
    dataToParse.remove_prefix(bytesParsed);

    ASSERT_TRUE(reader.parseBytes(dataToParse, &bytesParsed));
    ASSERT_EQ(http::HttpStreamReader::ReadState::readingMessageBody, reader.state());
    ASSERT_EQ(kMessageBody.size(), bytesParsed);
    dataToParse.remove_prefix(bytesParsed);
}

//-------------------------------------------------------------------------------------------------

TEST_F(HttpStreamReader, any_request_body_is_parsed_properly)
{
    addRequestToInputStream({.method = Method::get, .body = "Body"});
    addRequestToInputStream({.method = Method::get});
    addRequestToInputStream({.method = Method::delete_, .body = "Another body"});

    parseInputStream();

    assertParseSucceeds();
    assertParsedMessageStreamMathesInput();
}

TEST_F(HttpStreamReader, body_in_response_of_any_status_is_parsed_properly)
{
    addResponseToInputStream({.status = StatusCode::noContent, .body = "Body"});
    addResponseToInputStream({.status = StatusCode::noContent});
    addResponseToInputStream({.status = StatusCode::switchingProtocols, .body = "Another body"});

    parseInputStream();

    assertParseSucceeds();
    assertParsedMessageStreamMathesInput();
}

TEST_F(HttpStreamReader, parsing_can_be_resumed_after_failure)
{
    const nx::Buffer data =
        "INVALID_REQUEST\r\n"
        "GET /abc/def/ghi/jkl/mn/oi/prs/tquv/wxyz HTTP/1.1\r\n"
        "Content-Length: 0\r\n"
        "X-Req: 1\r\n"
        "\r\n";

    std::vector<Message> messages;
    parseBufWithFixedReadStep(
        data, 5, [&messages](auto& /*reader*/, const auto& msg) { messages.push_back(msg); },
        /*ignoreParseErrors*/ true);

    ASSERT_EQ(1, messages.size());
    ASSERT_EQ(Method::get, messages[0].request->requestLine.method);
    ASSERT_EQ("/abc/def/ghi/jkl/mn/oi/prs/tquv/wxyz", messages[0].request->requestLine.url);
    ASSERT_EQ(http_1_1, messages[0].request->requestLine.version);
    ASSERT_EQ(2, messages[0].request->headers.size());
}

} // namespace nx::network::http::test
