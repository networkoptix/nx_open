// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/server/rest/base_request_handler.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::http::server::rest::test {

namespace {

struct Input
{
    std::string s;
};

#define Input_Fields (s)

NX_REFLECTION_INSTRUMENT(Input, Input_Fields)

struct Output
{
    std::string s;
};

#define Output_Fields (s)

NX_REFLECTION_INSTRUMENT(Output, Output_Fields)

struct Result
{
    bool ok = true;
};

static void convert(const Result& result, ApiRequestResult* httpResult)
{
    if (result.ok)
        *httpResult = ApiRequestResult();
    else
        httpResult->setErrorClass(ApiRequestErrorClass::logicError);
}

} // namespace

//-------------------------------------------------------------------------------------------------

static constexpr char kCallWithInputOutputPath[] = "/test/{restParam}/";
static constexpr char kParam[] = "restParam";

static constexpr char kCallWithHttpStatusCodeAsResultPath[] = "/test/http-status-as-result/";

class HttpServerRestRequestHandler:
    public ::testing::Test
{
    using base_type = ::testing::Test;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        registerHandler<RequestHandler<Result, Input, Output, RestArgFetcher<kParam>>>(
            network::http::Method::put,
            kCallWithInputOutputPath);

        registerHandler<RequestHandler<StatusCode::Value, void, void>>(
            network::http::Method::delete_,
            kCallWithHttpStatusCodeAsResultPath);

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void whenInvokeCallWithInputAndOutputOverHttp()
    {
        m_expectedInput.s = nx::utils::generateRandomName(7);

        FusionDataHttpClient<Input, Output> client(
            generateRequestUrl(http::rest::substituteParameters(kCallWithInputOutputPath, {kParam})),
            Credentials(), ssl::kAcceptAnyCertificate, m_expectedInput);
        client.setRequestTimeout(kNoTimeout);

        invokeOverHttp(&client, Method::put);
    }

    void whenInvokeCallWithHttpStatusCodeAsResultOverHttp()
    {
        FusionDataHttpClient<void, void> client(
            generateRequestUrl(kCallWithHttpStatusCodeAsResultPath),
            Credentials(), ssl::kAcceptAnyCertificate);
        client.setRequestTimeout(kNoTimeout);

        invokeOverHttp(&client, Method::delete_);
    }

    void thenResponseIsReceived()
    {
        m_lastResponse = m_responseQueue.pop();
    }

    void andRequestSucceeded()
    {
        ASSERT_EQ(StatusCode::ok, m_lastResponse->statusCode);
    }

    void andInputWasReceived()
    {
        ASSERT_EQ(m_expectedInput.s, m_requestQueue.pop().input.s);
    }

    void andOutputIsReceived()
    {
        ASSERT_EQ(m_expectedOutput.s, m_lastResponse->output.s);
    }

private:
    struct RequestContext
    {
        std::string restParam;
        Input input;
    };

    struct ResponseContext
    {
        int statusCode;
        Output output;

        ResponseContext(int statusCode): statusCode(statusCode) {}

        ResponseContext(int statusCode, Output output):
            statusCode(statusCode), output(std::move(output))
        {
        }
    };

    TestHttpServer m_httpServer;
    Input m_expectedInput;
    Output m_expectedOutput;
    nx::utils::SyncQueue<RequestContext> m_requestQueue;
    nx::utils::SyncQueue<ResponseContext> m_responseQueue;
    std::optional<ResponseContext> m_lastResponse;

    nx::utils::Url generateRequestUrl(const std::string& path)
    {
        return nx::network::url::Builder()
            .setScheme(kUrlSchemeName).setEndpoint(m_httpServer.serverAddress())
            .setPath(path);
    }

    template<typename Handler>
    void registerHandler(const Method& method, const std::string& path)
    {
        m_httpServer.registerRequestProcessor(
            path,
            [this]()
            {
                return std::make_unique<Handler>(
                    [this](auto&&... args)
                    {
                        this->apiCall(std::forward<decltype(args)>(args)...);
                    });
            },
            method);
    }

    void apiCall(
        const std::string& restParam,
        const Input& input,
        nx::utils::MoveOnlyFunc<void(Result, Output)> handler)
    {
        m_requestQueue.push(RequestContext{restParam, input});
        m_expectedOutput.s = nx::utils::generateRandomName(7);

        handler(Result{true}, m_expectedOutput);
    }

    void apiCall(
        nx::utils::MoveOnlyFunc<void(StatusCode::Value)> handler)
    {
        handler(StatusCode::ok);
    }

    template<typename... OutputArgs>
    void saveResponse(
        SystemError::ErrorCode,
        const nx::network::http::Response* response,
        OutputArgs&&... output)
    {
        m_responseQueue.push(ResponseContext(
            response->statusLine.statusCode,
            std::forward<OutputArgs>(output)...));
    }

    template<typename Client>
    void invokeOverHttp(Client* client, Method method)
    {
        std::promise<void> done;

        client->execute(
            method,
            [this, &done](auto&&... args)
            {
                this->saveResponse(std::forward<decltype(args)>(args)...);
                done.set_value();
            });

        done.get_future().wait();

        client->pleaseStopSync();
    }
};

TEST_F(HttpServerRestRequestHandler, call_with_input_and_output)
{
    whenInvokeCallWithInputAndOutputOverHttp();

    thenResponseIsReceived();
    andRequestSucceeded();
    andInputWasReceived();
    andOutputIsReceived();
}

TEST_F(HttpServerRestRequestHandler, call_with_http_status_as_result)
{
    whenInvokeCallWithHttpStatusCodeAsResultOverHttp();

    thenResponseIsReceived();
    andRequestSucceeded();
}

} // namespace nx::network::http::server::rest::test
