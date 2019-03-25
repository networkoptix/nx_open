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

#include "tunneling_acceptance_tests.h"

namespace nx::network::http::tunneling::test {

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

TYPED_TEST_P(HttpTunneling, custom_http_headers_are_transferred)
{
    this->givenTunnellingServer();
    this->addSomeCustomHttpHeadersToTheClient();
    this->whenRequestTunnel();

    this->thenTunnelIsEstablished();
    this->andCustomHeadersAreReportedToTheServer();
}

REGISTER_TYPED_TEST_CASE_P(HttpTunneling,
    tunnel_is_established,
    error_is_reported,
    timeout_supported,
    custom_http_headers_are_transferred);

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

struct ValidatorBehavior
{
    ResultCode expectedResult = ResultCode::ok;
    bool failWithTimeout = false;
};

class TestValidator:
    public BaseTunnelValidator
{
    using base_type = BaseTunnelValidator;

public:
    TestValidator(
        std::unique_ptr<AbstractStreamSocket> tunnel,
        ValidatorBehavior behaviorDescriptor,
        nx::utils::SyncQueue<ResultCode>* validationResults)
        :
        base_type(std::move(tunnel)),
        m_behaviorDescriptor(behaviorDescriptor),
        m_validationResults(validationResults)
    {
    }

    virtual void setTimeout(std::chrono::milliseconds timeout) override
    {
        m_timeout = timeout;
    }

    virtual void validate(ValidateTunnelCompletionHandler handler) override
    {
        if (m_behaviorDescriptor.failWithTimeout && !m_timeout)
            return;

        m_validationResults->push(m_behaviorDescriptor.expectedResult);
        post([this, handler = std::move(handler)]()
            { handler(m_behaviorDescriptor.expectedResult); });
    }

private:
    ValidatorBehavior m_behaviorDescriptor;
    nx::utils::SyncQueue<ResultCode>* m_validationResults = nullptr;
    std::optional<std::chrono::milliseconds> m_timeout;
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
        m_validatorBehavior.expectedResult = ResultCode::ok;
        setTunnelValidator();
    }

    void givenFailingValidator()
    {
        m_validatorBehavior.expectedResult = ResultCode::ioError;
        setTunnelValidator();
    }

    void givenValidatorFailingTimeout()
    {
        m_validatorBehavior.expectedResult = ResultCode::ioError;
        m_validatorBehavior.failWithTimeout = true;
        setTunnelValidator();
    }

    void andValidationIsCompleted()
    {
        const auto result = m_validationResults.pop(std::chrono::milliseconds::zero());
        ASSERT_TRUE(result);
        ASSERT_EQ(m_validatorBehavior.expectedResult, *result);
    }

    void andTunnelTypePrioritiesChanged()
    {
        ASSERT_NE(m_initialTopTunnelTypeId, tunnelClientFactory().topTunnelTypeId(kTunnelTag));
    }

private:
    int m_initialTopTunnelTypeId = -1;
    nx::utils::SyncQueue<ResultCode> m_validationResults;
    ValidatorBehavior m_validatorBehavior;

    void setTunnelValidator()
    {
        tunnelingClient().setTunnelValidatorFactory(
            [this](auto tunnel, const Response&)
            {
                return std::make_unique<TestValidator>(
                    std::move(tunnel),
                    m_validatorBehavior,
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

TEST_F(HttpTunnelingValidation, client_forwards_timeout_to_validator)
{
    givenValidatorFailingTimeout();

    whenRequestTunnel();

    thenTunnelIsNotEstablished();
    andValidationIsCompleted();
}

} // namespace nx::network::http::tunneling::test
