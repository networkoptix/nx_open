#include <memory>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/http/tunneling/base_tunnel_validator.h>
#include <nx/network/http/tunneling/client.h>
#include <nx/network/http/tunneling/server.h>
#include <nx/network/http/tunneling/detail/client_factory.h>
#include <nx/network/http/tunneling/detail/connection_upgrade_tunnel_client.h>
#include <nx/network/http/tunneling/detail/get_post_tunnel_client.h>
#include <nx/network/http/tunneling/detail/experimental_tunnel_client.h>
#include <nx/network/http/tunneling/detail/ssl_tunnel_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::http::tunneling::test {

enum TunnelMethod
{
    getPost = 0x01,
    connectionUpgrade = 0x02,
    experimental = 0x04,
    ssl = 0x08,

    all = getPost | connectionUpgrade | experimental | ssl,
};

static constexpr char kBasePath[] = "/HttpTunnelingTest";
static constexpr char kTunnelTag[] = "/HttpTunnelingTest";

template<typename TunnelMethodMask>
class HttpTunneling:
    public ::testing::Test
{
public:
    HttpTunneling():
        m_tunnelingServer(
            std::bind(&HttpTunneling::saveServerTunnel, this, std::placeholders::_1),
            nullptr)
    {
        m_clientFactoryBak = detail::ClientFactory::instance().setCustomFunc(
            [this](const std::string& tag, const nx::utils::Url& baseUrl)
            {
                return m_localFactory.create(tag, baseUrl);
            });

        enableTunnelMethods(TunnelMethodMask::value);
    }

    ~HttpTunneling()
    {
        detail::ClientFactory::instance().setCustomFunc(
            std::move(m_clientFactoryBak));

        if (m_tunnelingClient)
            m_tunnelingClient->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        startHttpServer();

        m_tunnelingClient = std::make_unique<Client>(m_baseUrl, kTunnelTag);
    }

    void stopTunnelingServer()
    {
        m_httpServer->server().pleaseStopSync();
    }

    void setEstablishTunnelTimeout(std::chrono::milliseconds timeout)
    {
        m_timeout = timeout;
    }

    void givenTunnellingServer()
    {
        m_tunnelingServer.registerRequestHandlers(
            kBasePath,
            &m_httpServer->httpMessageDispatcher());
    }

    void givenSilentTunnellingServer()
    {
        m_httpServer->registerRequestProcessorFunc(
            http::kAnyPath,
            [this](auto&&... args) { leaveRequestWithoutResponse(std::move(args)...); });
    }

    void whenRequestTunnel()
    {
        if (m_timeout)
            m_tunnelingClient->setTimeout(*m_timeout);
        else
            m_tunnelingClient->setTimeout(nx::network::kNoTimeout);

        m_tunnelingClient->openTunnel(
            [this](auto&&... args) { saveClientTunnel(std::move(args)...); });
    }

    void thenTunnelIsEstablished()
    {
        thenClientTunnelSucceeded();

        thenServerTunnelSucceded();

        assertDataExchangeWorksThroughTheTunnel();
    }

    void thenClientTunnelSucceeded()
    {
        m_prevClientTunnelResult = m_clientTunnels.pop();

        ASSERT_EQ(ResultCode::ok, m_prevClientTunnelResult.resultCode);
        ASSERT_EQ(SystemError::noError, m_prevClientTunnelResult.sysError);
        ASSERT_TRUE(StatusCode::isSuccessCode(m_prevClientTunnelResult.httpStatus));
        ASSERT_NE(nullptr, m_prevClientTunnelResult.connection);
    }

    void thenServerTunnelSucceded()
    {
        m_prevServerTunnelConnection = m_serverTunnels.pop();
        ASSERT_NE(nullptr, m_prevServerTunnelConnection);
    }

    void thenTunnelIsNotEstablished()
    {
        m_prevClientTunnelResult = m_clientTunnels.pop();

        ASSERT_NE(ResultCode::ok, m_prevClientTunnelResult.resultCode);
        // NOTE: sysError and httpStatus of m_prevClientTunnelResult may be ok.
        ASSERT_EQ(nullptr, m_prevClientTunnelResult.connection);
    }

    void andResultCodeIs(SystemError::ErrorCode expected)
    {
        ASSERT_EQ(expected, m_prevClientTunnelResult.sysError);
    }

    Client& tunnelingClient()
    {
        return *m_tunnelingClient;
    }

    detail::ClientFactory& tunnelClientFactory()
    {
        return m_localFactory;
    }

private:
    Server<> m_tunnelingServer;
    std::unique_ptr<Client> m_tunnelingClient;
    detail::ClientFactory m_localFactory;
    detail::ClientFactory::Function m_clientFactoryBak;
    nx::utils::SyncQueue<OpenTunnelResult> m_clientTunnels;
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>> m_serverTunnels;
    std::unique_ptr<TestHttpServer> m_httpServer;
    OpenTunnelResult m_prevClientTunnelResult;
    nx::utils::Url m_baseUrl;
    std::unique_ptr<AbstractStreamSocket> m_prevServerTunnelConnection;
    std::optional<std::chrono::milliseconds> m_timeout;

    void startHttpServer()
    {
        m_httpServer = std::make_unique<TestHttpServer>();

        ASSERT_TRUE(m_httpServer->bindAndListen());
        m_baseUrl = nx::network::url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_httpServer->serverAddress())
            .setPath(kBasePath);
    }

    void enableTunnelMethods(int tunnelMethodMask)
    {
        if ((tunnelMethodMask & TunnelMethod::all) == TunnelMethod::all)
            return; //< By default, the factory is initialized with all methods.

        m_localFactory.clear();

        if (tunnelMethodMask & TunnelMethod::getPost)
            m_localFactory.registerClientType<detail::GetPostTunnelClient>();

        if (tunnelMethodMask & TunnelMethod::connectionUpgrade)
            m_localFactory.registerClientType<detail::ConnectionUpgradeTunnelClient>();

        if (tunnelMethodMask & TunnelMethod::experimental)
            m_localFactory.registerClientType<detail::ExperimentalTunnelClient>();

        if (tunnelMethodMask & TunnelMethod::ssl)
            m_localFactory.registerClientType<detail::SslTunnelClient>();
    }

    void saveClientTunnel(OpenTunnelResult result)
    {
        m_clientTunnels.push(std::move(result));
    }

    void saveServerTunnel(std::unique_ptr<AbstractStreamSocket> connection)
    {
        m_serverTunnels.push(std::move(connection));
    }

    void assertDataExchangeWorksThroughTheTunnel()
    {
        ASSERT_TRUE(m_prevServerTunnelConnection->setNonBlockingMode(false));
        ASSERT_TRUE(m_prevClientTunnelResult.connection->setNonBlockingMode(false));

        assertDataCanBeTransferred(
            m_prevServerTunnelConnection.get(),
            m_prevClientTunnelResult.connection.get());

        assertDataCanBeTransferred(
            m_prevClientTunnelResult.connection.get(),
            m_prevServerTunnelConnection.get());
    }

    void assertDataCanBeTransferred(
        AbstractStreamSocket* from,
        AbstractStreamSocket* to)
    {
        constexpr char buf[] = "Hello, world!";
        ASSERT_EQ(sizeof(buf), from->send(buf, sizeof(buf)));

        char readBuf[sizeof(buf)];
        ASSERT_EQ(sizeof(readBuf), to->recv(readBuf, sizeof(readBuf)));

        ASSERT_EQ(0, memcmp(buf, readBuf, sizeof(readBuf)));
    }

    void leaveRequestWithoutResponse(
        http::RequestContext /*requestContext*/,
        http::RequestProcessedHandler /*completionHandler*/)
    {
    }
};

TYPED_TEST_CASE_P(HttpTunneling);

TYPED_TEST_P(HttpTunneling, tunnel_is_established)
{
    this->givenTunnellingServer();
    this->whenRequestTunnel();
    this->thenTunnelIsEstablished();
}

TYPED_TEST_P(HttpTunneling, error_is_reported)
{
    this->stopTunnelingServer();

    this->whenRequestTunnel();
    this->thenTunnelIsNotEstablished();
}

TYPED_TEST_P(HttpTunneling, timeout_supported)
{
    this->setEstablishTunnelTimeout(std::chrono::milliseconds(1));

    this->givenSilentTunnellingServer();

    this->whenRequestTunnel();

    this->thenTunnelIsNotEstablished();
    this->andResultCodeIs(SystemError::timedOut);
}

REGISTER_TYPED_TEST_CASE_P(HttpTunneling,
    tunnel_is_established,
    error_is_reported,
    timeout_supported);

//-------------------------------------------------------------------------------------------------

struct EveryMethodMask { static constexpr int value = TunnelMethod::all; };
INSTANTIATE_TYPED_TEST_CASE_P(EveryMethod, HttpTunneling, EveryMethodMask);

struct GetPostMethodMask { static constexpr int value = TunnelMethod::getPost; };
INSTANTIATE_TYPED_TEST_CASE_P(GetPost, HttpTunneling, GetPostMethodMask);

struct ConnectionUpgradeMethodMask { static constexpr int value = TunnelMethod::connectionUpgrade; };
INSTANTIATE_TYPED_TEST_CASE_P(ConnectionUpgrade, HttpTunneling, ConnectionUpgradeMethodMask);

struct ExperimentalMethodMask { static constexpr int value = TunnelMethod::experimental; };
INSTANTIATE_TYPED_TEST_CASE_P(Experimental, HttpTunneling, ExperimentalMethodMask);

struct SslMethodMask { static constexpr int value = TunnelMethod::ssl; };
INSTANTIATE_TYPED_TEST_CASE_P(Ssl, HttpTunneling, SslMethodMask);

//-------------------------------------------------------------------------------------------------

namespace {

class TestValidator:
    public BaseTunnelValidator
{
    using base_type = BaseTunnelValidator;

public:
    TestValidator(
        std::unique_ptr<AbstractStreamSocket> tunnel,
        ResultCode resultCode,
        nx::utils::SyncQueue<ResultCode>* validationResults)
        :
        base_type(std::move(tunnel)),
        m_resultCode(resultCode),
        m_validationResults(validationResults)
    {
    }

    virtual void validate(ValidateTunnelCompletionHandler handler) override
    {
        m_validationResults->push(m_resultCode);
        post([this, handler = std::move(handler)]() { handler(m_resultCode); });
    }

private:
    ResultCode m_resultCode = ResultCode::ok;
    nx::utils::SyncQueue<ResultCode>* m_validationResults = nullptr;
};

} // namespace

class HttpTunnelingValidation:
    public HttpTunneling<EveryMethodMask>
{
    using base_type = HttpTunneling<EveryMethodMask>;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenTunnellingServer();

        m_initialTopTunnelTypeId = tunnelClientFactory().topTunnelTypeId(kTunnelTag);
    }

    void givenSuccessfulValidator()
    {
        m_expectedResult = ResultCode::ok;
        setTunnelValidator();
    }

    void givenFailingValidator()
    {
        m_expectedResult = ResultCode::ioError;
        setTunnelValidator();
    }

    void andValidationIsCompleted()
    {
        const auto result = m_validationResults.pop(std::chrono::milliseconds::zero());
        ASSERT_TRUE(result);
        ASSERT_EQ(m_expectedResult, *result);
    }

    void andTunnelTypePrioritiesChanged()
    {
        ASSERT_NE(m_initialTopTunnelTypeId, tunnelClientFactory().topTunnelTypeId(kTunnelTag));
    }

private:
    int m_initialTopTunnelTypeId = -1;
    nx::utils::SyncQueue<ResultCode> m_validationResults;
    ResultCode m_expectedResult = ResultCode::ok;

    void setTunnelValidator()
    {
        tunnelingClient().setTunnelValidatorFactory(
            [this](auto tunnel)
            {
                return std::make_unique<TestValidator>(
                    std::move(tunnel),
                    m_expectedResult,
                    &m_validationResults);
            });
    }
};

TEST_F(HttpTunnelingValidation, validation_is_performed)
{
    givenSuccessfulValidator();

    whenRequestTunnel();

    thenTunnelIsEstablished();
    andValidationIsCompleted();
}

TEST_F(HttpTunnelingValidation, validation_error_is_reported)
{
    givenFailingValidator();

    whenRequestTunnel();

    thenTunnelIsNotEstablished();
    andValidationIsCompleted();
    andTunnelTypePrioritiesChanged();
}

} // namespace nx::network::http::tunneling::test
