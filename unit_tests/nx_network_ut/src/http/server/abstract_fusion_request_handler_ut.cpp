#include <gtest/gtest.h>

#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/utils/thread/sync_queue.h>

#include "test_data.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace test {

class TestHandler:
    public nx::network::http::AbstractFusionRequestHandler<void, Serializable>
{
public:
    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        const nx::network::http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/) override
    {
        nx::network::http::FusionRequestResult result;
        result.setHttpStatusCode(m_httpStatusCode);

        Serializable output;
        output.dummyInt = 47;
        requestCompleted(result, output);
    }

    void setHttpStatusCode(nx::network::http::StatusCode::Value httpStatusCode)
    {
        m_httpStatusCode = httpStatusCode;
    }

private:
    nx::network::http::StatusCode::Value m_httpStatusCode = nx::network::http::StatusCode::ok;
};

//-------------------------------------------------------------------------------------------------

class HttpServerAbstractFusionRequestHandler:
    public ::testing::Test
{
protected:
    void whenRequestCompletedSuccessfully()
    {
        invokeProcessRequest(nx::network::http::StatusCode::ok);
    }

    void whenRequestProducedStatusCodeForbiddingMessageBody()
    {
        invokeProcessRequest(nx::network::http::StatusCode::switchingProtocols);
    }

    void thenOutputIsSerializedToMessageBody()
    {
        const auto result = m_requestResults.pop();
        ASSERT_NE(nullptr, result.msgBody);
    }

    void thenOutputIsSerializedToResponseHeaders()
    {
        const auto result = m_requestResults.pop();
        ASSERT_EQ(nullptr, result.msgBody);

        test::Serializable output;
        ASSERT_TRUE(test::deserializeFromHeaders(
            result.responseMessage.response->headers, &output));
    }

private:
    struct Result
    {
        nx::network::http::Message responseMessage;
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody;
    };

    TestHandler m_handler;
    nx::network::http::Response m_response;
    nx::utils::SyncQueue<Result> m_requestResults;

    void invokeProcessRequest(nx::network::http::StatusCode::Value statusCode)
    {
        using namespace std::placeholders;

        m_handler.setHttpStatusCode(statusCode);

        nx::network::http::Message message(nx::network::http::MessageType::request);
        message.request->requestLine.method = nx::network::http::Method::get;
        message.request->requestLine.url = "/some/url";
        message.request->requestLine.version = nx::network::http::http_1_1;

        m_handler.AbstractHttpRequestHandler::processRequest(
            nullptr,
            std::move(message),
            nx::utils::stree::ResourceContainer(),
            std::bind(&HttpServerAbstractFusionRequestHandler::onRequestProcessed, this,
                _1, _2, _3));
    }

    void onRequestProcessed(
        nx::network::http::Message responseMessage,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody,
        ConnectionEvents /*connectionEvents*/)
    {
        Result result;
        result.responseMessage = std::move(responseMessage);
        result.msgBody = std::move(msgBody);
        m_requestResults.push(std::move(result));
    }
};

TEST_F(HttpServerAbstractFusionRequestHandler, success_result_output_serialized_to_message_body)
{
    whenRequestCompletedSuccessfully();
    thenOutputIsSerializedToMessageBody();
}

TEST_F(
    HttpServerAbstractFusionRequestHandler,
    success_result_output_serialized_to_response_headers_if_status_code_forbids_body)
{
    whenRequestProducedStatusCodeForbiddingMessageBody();
    thenOutputIsSerializedToResponseHeaders();
}

} // namespace test
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
