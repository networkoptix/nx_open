// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/generic_api_client.h>

#include "simple_service/service_launcher.h"

namespace nx::network::http::test {

namespace {
constexpr char kDefaultPath[] = "/simpleService/handler";
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

class SimpleServiceLauncher:
    public http::server::test::Launcher
{
public:
    virtual ~SimpleServiceLauncher() = default;

public:
    virtual void startInstance();
    virtual nx::utils::Url basicHttpUrl() const;
};

void SimpleServiceLauncher::startInstance()
{
    ASSERT_TRUE(startService(
        {"--http/listenOn=0.0.0.0:0",
         "--log/logger=file=-;level=WARNING",
         "--http/endpoints=0.0.0.0:0"}));
}

nx::utils::Url SimpleServiceLauncher::basicHttpUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost("127.0.0.1")
        .setPort(moduleInstance()->httpEndpoints()[0].port)
        .toUrl();
}

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

public:
    std::unique_ptr<Client> m_client;
    SimpleServiceLauncher m_instance;
    std::promise<ResultType> m_resultCode;

protected:
    virtual void SetUp() override
    {
        startInstanceAndClient();

        m_initialHttpClientCount = SocketGlobals::instance().debugCounters().httpClientConnectionCount;
    }

    void saveRequestResult(std::tuple<ResultType> response)
    {
        m_resultCode.set_value(std::get<0>(response));
    }

    void whenPerformApiCall()
    {
        m_client->makeAsyncCall<void>(
            nx::network::http::Method::get, kDefaultPath, [this](auto&&... args) {
                // Using post to make sure that the completion handler has returned before saving the result.
                m_client->post([this, args = std::make_tuple(std::forward<decltype(args)>(args)...)]() {
                    std::apply(
                        [this](auto&&... args) { saveRequestResult(std::forward<decltype(args)>(args)...); },
                        std::move(args));
                });
            });
    }

    void thenApiCallSucceeded()
    {
        ASSERT_EQ(nx::network::http::StatusCode::ok, m_resultCode.get_future().get());
    }

    void thenApiCallFail500()
    {
        ASSERT_EQ(
            nx::network::http::StatusCode::internalServerError, m_resultCode.get_future().get());
    }

    void andThenRetriesDone(int numRetries)
    {
        ASSERT_EQ(numRetries, m_instance.moduleInstance()->getCurAttempNum());
    }

    void andHttpClientCountIs(int count)
    {
        ASSERT_EQ(
            count,
            SocketGlobals::instance().debugCounters().httpClientConnectionCount - m_initialHttpClientCount);
    }

private:
    void startInstanceAndClient()
    {
        m_instance.startInstance();
        m_instance.moduleInstance()->httpEndpoints();
        m_instance.moduleInstance()->resetAttemptsNum();
        m_client = std::make_unique<Client>(
            nx::network::url::Builder(m_instance.basicHttpUrl()).toUrl());
    }

private:
    int m_initialHttpClientCount = 0;
};

TEST_F(GenericApiClient, testNumRetries)
{
    const int kNumRetries = 3;

    m_client->setRetryPolicy(kNumRetries, nx::network::http::StatusCode::ok);
    m_instance.moduleInstance()->setSuccessfullAttemptNum(kNumRetries);
    whenPerformApiCall();

    thenApiCallSucceeded();
    andThenRetriesDone(kNumRetries);
    andHttpClientCountIs(0);
}

TEST_F(GenericApiClient, testWithfailingRequest)
{
    const int kNumRetries = 2;
    m_client->setRetryPolicy(kNumRetries, nx::network::http::StatusCode::ok);
    m_instance.moduleInstance()->setSuccessfullAttemptNum(kNumRetries + 1);
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

} // namespace nx::network::http::test
