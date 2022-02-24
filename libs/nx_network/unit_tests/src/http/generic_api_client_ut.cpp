// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/generic_api_client.h>

#include "simple_service/service_launcher.h"

namespace nx::network::http::server::test {

namespace {
constexpr char kDefaultPath[] = "/simpleService/handler";
} // namespace

class GenericApiClientTests;

//added this class to open protected GenericApiClient functions for testing
class GenericApiClientChild: public GenericApiClient<>
{
    friend class GenericApiClientTests;

public:
    GenericApiClientChild(const utils::Url& baseApiUrl): GenericApiClient<>(baseApiUrl, nx::network::ssl::kAcceptAnyCertificate) {}
};

class SimpleServiceLauncher: public Launcher
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

class GenericApiClientTests: public ::testing::Test
{
    using Client = GenericApiClientChild;
    using ResultType = nx::network::http::StatusCode::Value;

public:
    ~GenericApiClientTests()
    {
        if (m_client)
            m_client->pleaseStopSync();
    }

protected:
    void whenStartInstanceAndClient()
    {
        m_instance.startInstance();
        m_instance.moduleInstance()->httpEndpoints();
        m_instance.moduleInstance()->resetAttemptsNum();
        m_client =
            std::make_unique<Client>(nx::network::url::Builder(m_instance.basicHttpUrl()).toUrl());
    }

    void saveRequestResult(std::tuple<ResultType> response)
    {
        m_resultCode.set_value(std::get<0>(response));
    }

    void whenPerformApiCall()
    {
        m_client->makeAsyncCall<void>(
            nx::network::http::Method::get, kDefaultPath, [this](auto&&... args) {
                saveRequestResult(std::forward<decltype(args)>(args)...);
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
    };

public:
    std::unique_ptr<Client> m_client;
    SimpleServiceLauncher m_instance;
    std::promise<ResultType> m_resultCode;
};

TEST_F(GenericApiClientTests, testNumRetries)
{
    const int kNumRetries = 3;

    whenStartInstanceAndClient();
    m_client->setRetryPolicy(kNumRetries, nx::network::http::StatusCode::ok);
    m_instance.moduleInstance()->setSuccessfullAttemptNum(kNumRetries);
    whenPerformApiCall();

    thenApiCallSucceeded();
    andThenRetriesDone(kNumRetries);
}

TEST_F(GenericApiClientTests, testWithfailingRequest)
{
    const int kNumRetries = 2;
    whenStartInstanceAndClient();
    m_client->setRetryPolicy(kNumRetries, nx::network::http::StatusCode::ok);
    m_instance.moduleInstance()->setSuccessfullAttemptNum(kNumRetries + 1);
    whenPerformApiCall();

    thenApiCallFail500();
    andThenRetriesDone(kNumRetries);
}

} // namespace nx::network::http::server::test
