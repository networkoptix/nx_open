// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/generic_api_client.h>
#include <nx/network/http/server/rest/base_request_handler.h>
#include <nx/network/http/test_http_server.h>

#include "simple_service/service_launcher.h"
#include "simple_service/view.h"

namespace nx::network::http::test {

namespace {

template<const char* name>
struct HeaderFetcher
{
    static constexpr std::string_view kName{name};

    std::string operator()(const http::Response& response)
    {
        auto it = response.headers.find(kName);
        return it != response.headers.end() ? it->second : "";
    }
};

struct TestReply
{
    nx::Buffer httpRequest;
};

NX_REFLECTION_INSTRUMENT(TestReply, (httpRequest));

} // namespace

//added this class to open protected GenericApiClient functions for testing
class GenericApiClientChild:
    public http::GenericApiClient<>
{
public:
    using base_type = http::GenericApiClient<>;

    GenericApiClientChild(const utils::Url& baseApiUrl):
        http::GenericApiClient<>(baseApiUrl, nx::network::ssl::kAcceptAnyCertificate)
    {}

    using base_type::makeAsyncCall;
};

static constexpr char kTestPath[] = "/test";

class GenericApiClient:
    public ::testing::Test
{
    using Client = GenericApiClientChild;
    using ResultType = nx::network::http::StatusCode::Value;

public:
    ~GenericApiClient()
    {
        if (m_client)
            m_client->pleaseStopSync();
    }

protected:
    using Result = std::tuple<ResultType, TestReply, std::string /*Server header*/>;

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_server.bindAndListen(SocketAddress::anyPrivateAddressV4));

        m_server.registerRequestProcessorFunc(kTestPath,
            [this](auto&&... args) { handleTestRequest(std::forward<decltype(args)>(args)...); });

        startInstanceAndClient();

        m_initialHttpClientCount = SocketGlobals::instance().debugCounters().httpClientConnectionCount;
    }

    void whenPerformApiCall()
    {
        static constexpr char kHeaderName[] = "Server";

        m_client->makeAsyncCall<TestReply, HeaderFetcher<kHeaderName>>(
            nx::network::http::Method::get,
            kTestPath,
            {}, //query
            [this](auto&&... args) {
                // Using post to make sure that the completion handler has returned before saving the result.
                m_client->post([this, args = std::make_tuple(std::forward<decltype(args)>(args)...)]() {
                    m_results.push(std::move(args));
                });
            });
    }

    void thenApiCallSucceeded()
    {
        m_lastResult = m_results.pop();

        ASSERT_EQ(nx::network::http::StatusCode::ok, std::get<0>(*m_lastResult));
    }

    void thenApiCallFail500()
    {
        m_lastResult = m_results.pop();

        ASSERT_EQ(nx::network::http::StatusCode::internalServerError, std::get<0>(*m_lastResult));
    }

    void andThenRetriesDone(int numRetries)
    {
        ASSERT_EQ(numRetries, m_requestsReceived);
    }

    void andHttpClientCountIs(int count)
    {
        ASSERT_EQ(
            count,
            SocketGlobals::instance().debugCounters().httpClientConnectionCount - m_initialHttpClientCount);
    }

    void andHttpRequestContainedHeaders(std::vector<HttpHeader> expectedHeaders)
    {
        Request request;
        ASSERT_TRUE(request.parse(std::get<1>(*m_lastResult).httpRequest));

        for (const auto& h: expectedHeaders)
            ASSERT_EQ(h.second, getHeaderValue(request.headers, h.first));
    }

    void setFailsBeforeSuccess(int requestsToFail)
    {
        m_requestsToFail = requestsToFail;
    }

    void setCustomHeaders(std::vector<HttpHeader> headers)
    {
        for (const auto& h: headers)
            m_client->httpClientOptions().addAdditionalHeader(h.first, h.second);
    }

protected:
    std::unique_ptr<Client> m_client;
    nx::network::http::TestHttpServer m_server;

private:
    void startInstanceAndClient()
    {
        m_client = std::make_unique<Client>(nx::network::url::Builder()
            .setScheme(kUrlSchemeName).setEndpoint(m_server.serverAddress()).toUrl());
    }

    void handleTestRequest(
        RequestContext ctx,
        RequestProcessedHandler handler)
    {
        ++m_requestsReceived;

        if (m_requestsToFail > 0)
        {
            --m_requestsToFail;
            handler(RequestResult(StatusCode::internalServerError));
            return;
        }

        TestReply reply;
        reply.httpRequest = ctx.request.toString();
        RequestResult result(StatusCode::ok);
        result.body = std::make_unique<BufferSource>(
            "application/json", nx::reflect::json::serialize(reply));
        handler(std::move(result));
    }

private:
    nx::utils::SyncQueue<Result> m_results;
    std::optional<Result> m_lastResult;
    int m_initialHttpClientCount = 0;
    std::atomic<int> m_requestsToFail = 0;
    std::atomic<int> m_requestsReceived = 0;
};

TEST_F(GenericApiClient, testNumRetries)
{
    const int kNumRetries = 3;

    m_client->setRetryPolicy(kNumRetries, nx::network::http::StatusCode::ok);
    setFailsBeforeSuccess(kNumRetries - 1);
    whenPerformApiCall();

    thenApiCallSucceeded();
    andThenRetriesDone(kNumRetries);
    andHttpClientCountIs(0);
}

TEST_F(GenericApiClient, testWithfailingRequest)
{
    const int kNumRetries = 2;
    m_client->setRetryPolicy(kNumRetries, nx::network::http::StatusCode::ok);
    setFailsBeforeSuccess(kNumRetries);
    whenPerformApiCall();

    thenApiCallFail500();
    andThenRetriesDone(kNumRetries);
}

TEST_F(GenericApiClient, internal_http_client_is_freed_after_request_completion)
{
    whenPerformApiCall();

    thenApiCallSucceeded();
    andHttpClientCountIs(0);
}

TEST_F(GenericApiClient, custom_headers_are_added_to_request)
{
    setCustomHeaders({{"X-Custom-Test", "foo"}});

    whenPerformApiCall();

    thenApiCallSucceeded();
    andHttpRequestContainedHeaders({{"X-Custom-Test", "foo"}});
}

//-------------------------------------------------------------------------------------------------

namespace {

struct TestResource
{
    std::string value;

    bool operator==(const TestResource&) const = default;
};

NX_REFLECTION_INSTRUMENT(TestResource, (value));

struct OtherTestResource
{
    std::string dummy1;
    std::string value;
    int dummy2 = 0;

    bool operator==(const OtherTestResource&) const = default;
};

NX_REFLECTION_INSTRUMENT(OtherTestResource, (value));

} // namespace

static constexpr char kCachableResourcePath[] = "/testResource";

class GenericApiClientWithCaching:
    public GenericApiClient
{
public:
    ~GenericApiClientWithCaching()
    {
        m_client->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        GenericApiClient::SetUp();

        m_client->setCacheEnabled(true);

        m_resource.value = nx::utils::generateRandomName(7);

        m_server.registerRequestProcessorFunc(kCachableResourcePath, [this](auto&&... args) {
            return getResource(std::forward<decltype(args)>(args)...);
        });
    }

    void givenClientWithCachedResource()
    {
        whenRequestResource();
        thenExpectedResourceWasProvided();
    }

    void whenRequestResource()
    {
        m_client->makeAsyncCall<TestResource>(
            nx::network::http::Method::get,
            kCachableResourcePath,
            {}, //query
            [this](auto&&... args) {
                m_results.push(std::make_tuple(std::forward<decltype(args)>(args)...));
            });
    }

    void whenRequestResourceOfDifferentType()
    {
        m_client->makeAsyncCall<OtherTestResource>(
            nx::network::http::Method::get,
            kCachableResourcePath,
            {}, //query
            [this](auto result, OtherTestResource reply) {
                m_results.push(std::make_tuple(result, TestResource{.value = reply.value}));
            });
    }

    void whenUpdateResource()
    {
        m_resource.value = nx::utils::generateRandomName(7);
    }

    void thenExpectedResourceWasProvided()
    {
        m_lastResult = m_results.pop();
        ASSERT_EQ(StatusCode::ok, std::get<0>(*m_lastResult));
        ASSERT_EQ(m_resource, std::get<1>(*m_lastResult));
    }

    void andConditionalRequestWasSent()
    {
        auto it = m_lastRequest.headers.find("If-None-Match");
        ASSERT_NE(m_lastRequest.headers.end(), it);
        ASSERT_EQ(std::get<1>(*m_lastResult).value, it->second);
    }

    void andCacheContainsRecentValue()
    {
        whenRequestResource();
        thenExpectedResourceWasProvided();
        andConditionalRequestWasSent();
    }

private:
    void getResource(
        RequestContext ctx,
        RequestProcessedHandler handler)
    {
        m_lastRequest = ctx.request;

        auto etagIt = ctx.request.headers.find("If-None-Match");
        if (etagIt != ctx.request.headers.end() && etagIt->second == m_resource.value)
        {
            handler(RequestResult(StatusCode::notModified));
            return;
        }

        RequestResult result(StatusCode::ok);
        result.body = std::make_unique<BufferSource>(
            "application/json", nx::reflect::json::serialize(m_resource));
        result.headers.emplace("ETag", m_resource.value);
        handler(std::move(result));
    }

private:
    using Result = std::tuple<StatusCode::Value, TestResource>;

    TestResource m_resource;
    nx::utils::SyncQueue<Result> m_results;
    std::optional<Result> m_lastResult;
    Request m_lastRequest;
};

TEST_F(GenericApiClientWithCaching, known_value_is_returned_from_cache)
{
    whenRequestResource();
    thenExpectedResourceWasProvided();
    // the client is expected to cache the result here.

    whenRequestResource();
    thenExpectedResourceWasProvided();
    andConditionalRequestWasSent();
}

TEST_F(GenericApiClientWithCaching, cache_item_is_ignored_if_value_of_different_type_is_expected)
{
    givenClientWithCachedResource();
    whenRequestResourceOfDifferentType();
    thenExpectedResourceWasProvided();
}

TEST_F(GenericApiClientWithCaching, cache_is_update_if_etag_not_matched)
{
    givenClientWithCachedResource();

    whenUpdateResource();

    whenRequestResource();
    thenExpectedResourceWasProvided();
    andCacheContainsRecentValue();
}

} // namespace nx::network::http::test
