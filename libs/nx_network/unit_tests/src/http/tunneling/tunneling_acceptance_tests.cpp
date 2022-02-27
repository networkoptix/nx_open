// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
#include <nx/utils/std/algorithm.h>
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

TYPED_TEST_P(HttpTunneling, client_supports_timeout)
{
    constexpr auto kEstablishTunnelTimeout = std::chrono::milliseconds(1);

    this->setEstablishTunnelTimeout(kEstablishTunnelTimeout);

    this->givenDelayingTunnellingServer(kEstablishTunnelTimeout * 10);

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

TYPED_TEST_P(HttpTunneling, does_not_crash_in_case_of_unexpected_connection_closure)
{
    this->addDelayToTunnelAuthorization(std::chrono::milliseconds(200));

    this->givenTunnellingServer();

    this->whenRequestTunnel();
    this->blockUntilTunnelAuthorizationIsRequested();
    this->deleteTunnelClient();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // thenProcessDoesNotCrash();
}

REGISTER_TYPED_TEST_SUITE_P(HttpTunneling,
    tunnel_is_established,
    error_is_reported,
    client_supports_timeout,
    custom_http_headers_are_transferred,
    does_not_crash_in_case_of_unexpected_connection_closure);

//-------------------------------------------------------------------------------------------------

struct EveryMethodMask { static constexpr int value = TunnelMethod::all; };
INSTANTIATE_TYPED_TEST_SUITE_P(EveryMethod, HttpTunneling, EveryMethodMask);

struct GetPostMethodMask { static constexpr int value = TunnelMethod::getPost; };
INSTANTIATE_TYPED_TEST_SUITE_P(GetPost, HttpTunneling, GetPostMethodMask);

struct ConnectionUpgradeMethodMask { static constexpr int value = TunnelMethod::connectionUpgrade; };
INSTANTIATE_TYPED_TEST_SUITE_P(ConnectionUpgrade, HttpTunneling, ConnectionUpgradeMethodMask);

struct ExperimentalMethodMask { static constexpr int value = TunnelMethod::experimental; };
INSTANTIATE_TYPED_TEST_SUITE_P(Experimental, HttpTunneling, ExperimentalMethodMask);

struct SslMethodMask { static constexpr int value = TunnelMethod::ssl; };
INSTANTIATE_TYPED_TEST_SUITE_P(Ssl, HttpTunneling, SslMethodMask);

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

    virtual void setTimeout(std::optional<std::chrono::milliseconds> timeout) override
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

        m_initialTopTunnelTypeIds = tunnelClientFactory().topTunnelTypeIds(kTunnelTag);
    }

    void enableReportingSilentConnectionAsTunnelingFailure()
    {
        tunnelingClient().setConsiderSilentConnectionAsTunnelFailure(true);
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

    void givenEstablishedTunnel()
    {
        givenSuccessfulValidator();

        whenRequestTunnel();

        thenClientTunnelSucceeded();
        thenServerTunnelSucceeded();
    }

    void thenTunnelTypePrioritiesChanged()
    {
        ASSERT_NE(m_initialTopTunnelTypeIds, tunnelClientFactory().topTunnelTypeIds(kTunnelTag));
    }

    void thenTunnelTypePrioritiesLeftUnchanged()
    {
        ASSERT_EQ(m_initialTopTunnelTypeIds, tunnelClientFactory().topTunnelTypeIds(kTunnelTag));
    }

    void andValidationIsCompleted()
    {
        const auto result = m_validationResults.pop(std::chrono::milliseconds::zero());
        ASSERT_TRUE(result);
        ASSERT_EQ(m_validatorBehavior.expectedResult, *result);
    }

    void andTunnelTypePrioritiesChanged()
    {
        ASSERT_NE(m_initialTopTunnelTypeIds, tunnelClientFactory().topTunnelTypeIds(kTunnelTag));
    }

private:
    std::vector<int> m_initialTopTunnelTypeIds;
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

TEST_F(
    HttpTunnelingValidation,
    connection_that_did_not_receive_any_data_is_considered_a_broken_tunnel)
{
    enableReportingSilentConnectionAsTunnelingFailure();

    givenEstablishedTunnel();
    whenCloseClientConnection();
    thenTunnelTypePrioritiesChanged();
}

TEST_F(
    HttpTunnelingValidation,
    connection_that_did_not_receive_any_data_is_not_considered_a_broken_tunnel_by_default)
{
    givenEstablishedTunnel();
    whenCloseClientConnection();
    thenTunnelTypePrioritiesLeftUnchanged();
}

TEST_F(
    HttpTunnelingValidation,
    tunnel_failure_is_not_reported_on_working_tunnel)
{
    enableReportingSilentConnectionAsTunnelingFailure();

    givenEstablishedTunnel();
    assertDataExchangeWorksThroughTheTunnel();

    whenCloseClientConnection();
    thenTunnelTypePrioritiesLeftUnchanged();
}

TEST_F(HttpTunnelingValidation, client_forwards_timeout_to_validator)
{
    givenValidatorFailingTimeout();

    whenRequestTunnel();

    thenTunnelIsNotEstablished();
    andValidationIsCompleted();
}

//-------------------------------------------------------------------------------------------------

namespace {

static std::optional<int> sWorkingTunnelTypeId; // All are allowed.

template<int tunnelTypeId>
class TunnelClient:
    public detail::GetPostTunnelClient
{
    using base_type = detail::GetPostTunnelClient;

public:
    using base_type::base_type;

    virtual void openTunnel(
        OpenTunnelCompletionHandler handler) override
    {
        if (sWorkingTunnelTypeId && tunnelTypeId != *sWorkingTunnelTypeId)
            return;

        base_type::openTunnel(std::move(handler));
    }
};

} // namespace

class HttpTunnelingClient:
    public HttpTunneling<EveryMethodMask>
{
    using base_type = HttpTunneling<EveryMethodMask>;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenTunnellingServer();
        registerTunnelTypes();
    }

    void andOnlyOneTunnelTypeHasHighestPriority()
    {
        ASSERT_EQ(1, tunnelClientFactory().topTunnelTypeIds(kTunnelTag).size());
    }

    std::unique_ptr<Client> initializeTunnelClient()
    {
        return std::make_unique<Client>(baseUrl(), kTunnelTag);
    }

    void setWorkingTunnelType(int tunnelTypeId)
    {
        sWorkingTunnelTypeId = tunnelTypeId;
    }

    void assertTunnelEstablished(Client& client)
    {
        whenRequestTunnel(&client);
        thenTunnelIsEstablished();
    }

    void assertTopTunnelTypeIsNot(int tunnelTypeIndex)
    {
        const auto tunnelIds = tunnelClientFactory().topTunnelTypeIds(kTunnelTag);
        ASSERT_FALSE(nx::utils::contains(tunnelIds, m_tunnelTypeIds[tunnelTypeIndex]));
    }

private:
    void registerTunnelTypes()
    {
        tunnelClientFactory().clear();

        m_tunnelTypeIds.clear();

        m_tunnelTypeIds.push_back(tunnelClientFactory().registerClientType<TunnelClient<0>>(0));
        m_tunnelTypeIds.push_back(tunnelClientFactory().registerClientType<TunnelClient<1>>(0));

        // Registering third type with a lower priority.
        m_tunnelTypeIds.push_back(tunnelClientFactory().registerClientType<TunnelClient<2>>(1));
    }

private:
    std::vector<int> m_tunnelTypeIds;
};

TEST_F(HttpTunnelingClient, the_first_tunnel_type_established_is_chosen_as_the_primary_one)
{
    whenRequestTunnel();

    thenTunnelIsEstablished();
    andOnlyOneTunnelTypeHasHighestPriority();
}

TEST_F(
    HttpTunnelingClient,
    concurrent_success_of_multiple_tunnels_does_not_result_all_tunnels_considered_as_failed)
{
    // If two tunnel connections are started concurrently at the start, then they all get
    // multiple same-priority tunnels (tunnel types 1 & 2).
    auto client1 = initializeTunnelClient();
    auto client2 = initializeTunnelClient();

    // When those complete, different available tunnels may have been used.
    setWorkingTunnelType(0);
    assertTunnelEstablished(*client1);

    setWorkingTunnelType(1);
    assertTunnelEstablished(*client2);

    // This MUST NOT result in a switch to a third tunnel type.
    assertTopTunnelTypeIsNot(2);
}

} // namespace nx::network::http::tunneling::test
