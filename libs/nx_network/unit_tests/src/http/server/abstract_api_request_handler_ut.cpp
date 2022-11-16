// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/server/abstract_api_request_handler.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/thread/sync_queue.h>

#include "test_data.h"

namespace nx::network::http::test {

template<typename TypeSet>
class HttpServerGenericApiHandler:
    public ::testing::Test
{
protected:
    template<typename... Args> using Handler =
        typename TypeSet::template Handler<Args...>;

    template<typename TypedHandler>
    class BaseContext
    {
    public:
        nx::network::http::Request m_request;
        TypedHandler m_handler;

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
            ASSERT_NE(nullptr, result.body);
        }

        void thenOutputIsSerializedToResponseHeaders()
        {
            const auto result = m_requestResults.pop();
            ASSERT_EQ(nullptr, result.body);

            test::Serializable output;
            ASSERT_TRUE(test::deserializeFromHeaders(result.headers, &output));
        }

        void thenResponseStatusCodeIs(StatusCode::Value expected)
        {
            auto requestResult = m_requestResults.pop();
            ASSERT_EQ(expected, requestResult.statusCode);
        }

    private:
        nx::utils::SyncQueue<RequestResult> m_requestResults;

        void invokeProcessRequest(nx::network::http::StatusCode::Value statusCode)
        {
            m_handler.setHttpStatusCode(statusCode);

            m_handler.serve(
                RequestContext(
                    {},
                    {},
                    SocketAddress(),
                    {},
                    m_request),
                [this](auto result) { m_requestResults.push(std::move(result)); });
        }
    };

    //-------------------------------------------------------------------------------------------------

    class OutHandler:
        public Handler<void /*no input*/>
    {
    public:
        virtual void processRequest(http::RequestContext /*requestContext*/) override
        {
            nx::network::http::ApiRequestResult result;
            result.setHttpStatusCode(m_httpStatusCode);

            this->requestCompleted(result, Serializable{47});
        }

        void setHttpStatusCode(nx::network::http::StatusCode::Value httpStatusCode)
        {
            m_httpStatusCode = httpStatusCode;
        }

    private:
        nx::network::http::StatusCode::Value m_httpStatusCode = nx::network::http::StatusCode::ok;
    };

    class OutOnlyContext:
        public BaseContext<OutHandler>
    {
    };

    //-------------------------------------------------------------------------------------------------

    class InOutHandler:
        public Handler<Serializable>
    {
    public:
        virtual void processRequest(
            http::RequestContext /*requestContext*/,
            Serializable input) override
        {
            m_lastInput = input;

            nx::network::http::ApiRequestResult result;
            result.setHttpStatusCode(m_httpStatusCode);

            this->requestCompleted(result, Serializable{47});
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

    class InOutContext:
        public BaseContext<InOutHandler>
    {
    public:
        void preparePostRequestCommon()
        {
            this->m_request.requestLine.method = nx::network::http::Method::post;
            this->m_request.requestLine.url = "/some/url";
            this->m_request.requestLine.version = nx::network::http::http_1_1;
        }

        void preparePostRequest()
        {
            preparePostRequestCommon();
            m_expectedInput.dummyInt = rand();
            this->m_request.messageBody = nx::reflect::json::serialize(m_expectedInput);
            this->m_request.headers.emplace("Content-Type", "application/json");
        }

        void preparePostRequestUrlencodedFormat()
        {
            preparePostRequestCommon();
            m_expectedInput.dummyInt = rand();
            this->m_request.messageBody = nx::reflect::urlencoded::serialize(m_expectedInput);
            this->m_request.headers.emplace("Content-Type", "application/x-www-form-urlencoded");
        }

        void preparePostRequestWithUnsupportedFormat()
        {
            preparePostRequestCommon();
            this->m_request.messageBody = "text";
            this->m_request.headers.emplace("Content-Type", "plain/text");
        }

        void thenInputIsDeserializedProperly()
        {
            ASSERT_EQ(m_expectedInput, this->m_handler.lastInput());
        }

    private:
        Serializable m_expectedInput;
    };

    //-------------------------------------------------------------------------------------------------

    class NoDataHandler:
        public Handler<void>
    {
    public:
        virtual void processRequest(http::RequestContext /*requestContext*/) override
        {
            nx::network::http::ApiRequestResult result;
            result.setHttpStatusCode(m_httpStatusCode);

            this->requestCompleted(result);
        }

        void setHttpStatusCode(nx::network::http::StatusCode::Value httpStatusCode)
        {
            m_httpStatusCode = httpStatusCode;
        }

    private:
        nx::network::http::StatusCode::Value m_httpStatusCode = nx::network::http::StatusCode::ok;
    };

    class NoDataContext:
        public BaseContext<NoDataHandler>
    {
    public:
        void assertRequestCompletedWithStatus(Method method, StatusCode::Value status)
        {
            this->m_request.requestLine.method = method;
            this->m_request.requestLine.url = "/some/url";
            this->m_request.requestLine.version = nx::network::http::http_1_1;

            this->whenSendRequest();
            this->thenResponseStatusCodeIs(status);
        }
    };
};

TYPED_TEST_SUITE_P(HttpServerGenericApiHandler);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(
    HttpServerGenericApiHandler,
    out_only_success_result_output_serialized_to_message_body)
{
    typename std::decay_t<decltype(*this)>::OutOnlyContext ctx;

    ctx.prepareGetRequest();
    ctx.whenRequestCompletedSuccessfully();
    ctx.thenOutputIsSerializedToMessageBody();
}

TYPED_TEST_P(
    HttpServerGenericApiHandler,
    out_only_success_result_output_serialized_to_response_headers_if_status_code_forbids_body)
{
    typename std::decay_t<decltype(*this)>::OutOnlyContext ctx;

    ctx.prepareGetRequest();
    ctx.whenRequestProducedStatusCodeForbiddingMessageBody();
    ctx.thenOutputIsSerializedToResponseHeaders();
}

TYPED_TEST_P(HttpServerGenericApiHandler, in_out_json_input_is_read_from_msg_body)
{
    typename std::decay_t<decltype(*this)>::InOutContext ctx;

    ctx.preparePostRequest();
    ctx.whenSendRequest();
    ctx.thenInputIsDeserializedProperly();
}

TYPED_TEST_P(HttpServerGenericApiHandler, in_out_urlencoded_input_is_read_from_msg_body)
{
    typename std::decay_t<decltype(*this)>::InOutContext ctx;

    ctx.preparePostRequestUrlencodedFormat();
    ctx.whenSendRequest();
    ctx.thenInputIsDeserializedProperly();
}

TYPED_TEST_P(HttpServerGenericApiHandler, in_out_unsupported_input_format_results_in_error)
{
    typename std::decay_t<decltype(*this)>::InOutContext ctx;

    ctx.preparePostRequestWithUnsupportedFormat();
    ctx.whenSendRequest();
    ctx.thenResponseStatusCodeIs(StatusCode::notAcceptable);
}

// TYPED_TEST_P(HttpServerGenericApiHandler, input_is_read_from_url_query)

TYPED_TEST_P(HttpServerGenericApiHandler, DISABLED_no_data_any_request_with_body_produces_an_error)
{
    typename std::decay_t<decltype(*this)>::NoDataContext ctx;

    ctx.assertRequestCompletedWithStatus(Method::post, StatusCode::notAllowed);
    ctx.assertRequestCompletedWithStatus(Method::put, StatusCode::notAllowed);
}

TYPED_TEST_P(HttpServerGenericApiHandler, no_data_request_with_no_body_is_handled)
{
    typename std::decay_t<decltype(*this)>::NoDataContext ctx;

    ctx.assertRequestCompletedWithStatus(Method::connect, StatusCode::ok);
    ctx.assertRequestCompletedWithStatus(Method::get, StatusCode::ok);
    ctx.assertRequestCompletedWithStatus(Method::delete_, StatusCode::ok);
}

REGISTER_TYPED_TEST_SUITE_P(HttpServerGenericApiHandler,
    out_only_success_result_output_serialized_to_message_body,
    out_only_success_result_output_serialized_to_response_headers_if_status_code_forbids_body,

    in_out_json_input_is_read_from_msg_body,
    in_out_urlencoded_input_is_read_from_msg_body,
    in_out_unsupported_input_format_results_in_error,

    DISABLED_no_data_any_request_with_body_produces_an_error,
    no_data_request_with_no_body_is_handled
);

//-------------------------------------------------------------------------------------------------

struct FusionTypes
{
    template<typename... Args> using Handler =
        nx::network::http::AbstractApiRequestHandler<Args...>;
};

INSTANTIATE_TYPED_TEST_SUITE_P(
    Fusion,
    HttpServerGenericApiHandler,
    FusionTypes);

} // namespace nx::network::http::test
