#include <gtest/gtest.h>

#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/utils/thread/sync_queue.h>

#include "test_data.h"

namespace nx::network::http::test {

template<typename Handler>
class HttpServerAbstractFusionRequestHandler:
    public ::testing::Test
{
protected:
    nx::network::http::Request m_request;
    Handler m_handler;

    void prepareGetRequest()
    {
        m_request.requestLine.method = nx::network::http::Method::get;
        m_request.requestLine.url = "/some/url";
        m_request.requestLine.version = nx::network::http::http_1_1;
    }

    void whenSendRequest()
    {
        invokeProcessRequest(nx::network::http::StatusCode::ok);
    }

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

    void thenResponseStatusCodeIs(StatusCode::Value expected)
    {
        auto requestResult = m_requestResults.pop();
        ASSERT_EQ(expected, requestResult.responseMessage.response->statusLine.statusCode);
    }

private:
    struct Result
    {
        nx::network::http::Message responseMessage;
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody;
    };

    nx::utils::SyncQueue<Result> m_requestResults;

    void invokeProcessRequest(nx::network::http::StatusCode::Value statusCode)
    {
        m_handler.setHttpStatusCode(statusCode);

        m_handler.AbstractHttpRequestHandler::processRequest(
            nullptr,
            m_request,
            nx::utils::stree::ResourceContainer(),
            [this](auto&&... args) { onRequestProcessed(std::forward<decltype(args)>(args)...); });
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

//-------------------------------------------------------------------------------------------------

namespace {

class OutHandler:
    public nx::network::http::AbstractFusionRequestHandler<void, Serializable>
{
public:
    virtual void processRequest(http::RequestContext /*requestContext*/) override
    {
        nx::network::http::FusionRequestResult result;
        result.setHttpStatusCode(m_httpStatusCode);

        requestCompleted(result, Serializable{47});
    }

    void setHttpStatusCode(nx::network::http::StatusCode::Value httpStatusCode)
    {
        m_httpStatusCode = httpStatusCode;
    }

private:
    nx::network::http::StatusCode::Value m_httpStatusCode = nx::network::http::StatusCode::ok;
};

} // namespace

class HttpServerAbstractFusionRequestHandlerOutOnly:
    public HttpServerAbstractFusionRequestHandler<OutHandler>
{
};

TEST_F(
    HttpServerAbstractFusionRequestHandlerOutOnly,
    success_result_output_serialized_to_message_body)
{
    prepareGetRequest();
    whenRequestCompletedSuccessfully();
    thenOutputIsSerializedToMessageBody();
}

TEST_F(
    HttpServerAbstractFusionRequestHandlerOutOnly,
    success_result_output_serialized_to_response_headers_if_status_code_forbids_body)
{
    prepareGetRequest();
    whenRequestProducedStatusCodeForbiddingMessageBody();
    thenOutputIsSerializedToResponseHeaders();
}

//-------------------------------------------------------------------------------------------------

namespace {

class InOutHandler:
    public nx::network::http::AbstractFusionRequestHandler<Serializable, Serializable>
{
public:
    virtual void processRequest(
        http::RequestContext /*requestContext*/,
        Serializable input) override
    {
        m_lastInput = input;

        nx::network::http::FusionRequestResult result;
        result.setHttpStatusCode(m_httpStatusCode);

        requestCompleted(result, Serializable{47});
    }

    void setHttpStatusCode(nx::network::http::StatusCode::Value httpStatusCode)
    {
        m_httpStatusCode = httpStatusCode;
    }

    Serializable lastInput() const
    {
        return m_lastInput;
    }

private:
    nx::network::http::StatusCode::Value m_httpStatusCode = nx::network::http::StatusCode::ok;
    Serializable m_lastInput;
};

} // namespace

class HttpServerAbstractFusionRequestHandlerInOut:
    public HttpServerAbstractFusionRequestHandler<InOutHandler>
{
protected:
    void preparePostRequest()
    {
        m_request.requestLine.method = nx::network::http::Method::post;
        m_request.requestLine.url = "/some/url";
        m_request.requestLine.version = nx::network::http::http_1_1;

        m_expectedInput.dummyInt = rand();
        m_request.messageBody = QJson::serialized(m_expectedInput);
        m_request.headers.emplace("Content-Type", "application/json");
    }

    void preparePostRequestWithUnsupportedFormat()
    {
        m_request.requestLine.method = nx::network::http::Method::post;
        m_request.requestLine.url = "/some/url";
        m_request.requestLine.version = nx::network::http::http_1_1;

        m_request.messageBody = "text";
        m_request.headers.emplace("Content-Type", "plain/text");
    }

    void thenInputIsDeserializedProperly()
    {
        ASSERT_EQ(m_expectedInput, m_handler.lastInput());
    }

private:
    Serializable m_expectedInput;
};

TEST_F(HttpServerAbstractFusionRequestHandlerInOut, input_is_read_from_msg_body)
{
    preparePostRequest();
    whenSendRequest();
    thenInputIsDeserializedProperly();
}

TEST_F(HttpServerAbstractFusionRequestHandlerInOut, unsupported_input_format_results_in_error)
{
    preparePostRequestWithUnsupportedFormat();
    whenSendRequest();
    thenResponseStatusCodeIs(StatusCode::notAcceptable);
}

// TEST_F(HttpServerAbstractFusionRequestHandler, input_is_read_from_url_query)

//-------------------------------------------------------------------------------------------------

namespace {

class NoDataHandler:
    public nx::network::http::AbstractFusionRequestHandler<void, void>
{
public:
    virtual void processRequest(http::RequestContext /*requestContext*/) override
    {
        nx::network::http::FusionRequestResult result;
        result.setHttpStatusCode(m_httpStatusCode);

        requestCompleted(result);
    }

    void setHttpStatusCode(nx::network::http::StatusCode::Value httpStatusCode)
    {
        m_httpStatusCode = httpStatusCode;
    }

private:
    nx::network::http::StatusCode::Value m_httpStatusCode = nx::network::http::StatusCode::ok;
};

} // namespace

class HttpServerAbstractFusionRequestHandlerNoData:
    public HttpServerAbstractFusionRequestHandler<NoDataHandler>
{
protected:
    void assertRequestCompletedWithStatus(Method::ValueType method, StatusCode::Value status)
    {
        m_request.requestLine.method = method;
        m_request.requestLine.url = "/some/url";
        m_request.requestLine.version = nx::network::http::http_1_1;

        whenSendRequest();
        thenResponseStatusCodeIs(status);
    }
};

TEST_F(HttpServerAbstractFusionRequestHandlerNoData, DISABLED_any_request_with_body_produces_an_error)
{
    assertRequestCompletedWithStatus(Method::post, StatusCode::notAllowed);
    assertRequestCompletedWithStatus(Method::put, StatusCode::notAllowed);
}

TEST_F(HttpServerAbstractFusionRequestHandlerNoData, request_with_no_body_is_handled)
{
    assertRequestCompletedWithStatus(Method::connect, StatusCode::ok);
    assertRequestCompletedWithStatus(Method::get, StatusCode::ok);
    assertRequestCompletedWithStatus(Method::delete_, StatusCode::ok);
}

} // namespace nx::network::http::test
