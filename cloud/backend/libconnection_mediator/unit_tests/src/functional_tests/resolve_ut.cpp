#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>

#include <nx/cloud/mediator/test_support/mediaserver_emulator.h>
#include <nx/network/cloud/mediator_client_connections.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class Resolve:
    public MediatorFunctionalTest
{
    using base_type = MediatorFunctionalTest;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(startAndWaitUntilStarted());

        nx::network::stun::AbstractAsyncClient::Settings clientSettings;
        clientSettings.sendTimeout = nx::network::kNoTimeout;
        clientSettings.recvTimeout = nx::network::kNoTimeout;
        m_stunClient = std::make_shared<nx::network::stun::AsyncClientWithHttpTunneling>(clientSettings);
        m_stunClient->connect(httpTunnelUrl(), [](auto&&...) {});

        m_client = std::make_unique<api::MediatorClientTcpConnection>(m_stunClient);
    }

    virtual void TearDown() override
    {
        if (m_client)
        {
            m_client->pleaseStopSync();
            m_client.reset();
        }

        if (m_stunClient)
            m_stunClient->pleaseStopSync();
    }

    nx::hpm::api::MediatorClientTcpConnection& client()
    {
        return *m_client;
    }

private:
    std::shared_ptr<nx::network::stun::AsyncClientWithHttpTunneling> m_stunClient;
    std::unique_ptr<nx::hpm::api::MediatorClientTcpConnection> m_client;
};

TEST_F(Resolve, common)
{
    //TODO #ak adding multiple server to a system, checking that each very server is found

    using namespace nx::hpm;

    const auto system1 = addRandomSystem();
    auto system1Servers = addRandomServers(system1, 2);

    // resolve system
    {
        api::ResolveDomainResponse resolveResponse;
        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, resolveResponse) =
            makeSyncCall<api::ResultCode, api::ResolveDomainResponse>(std::bind(
                &nx::hpm::api::MediatorClientTcpConnection::resolveDomain,
                &client(),
                api::ResolveDomainRequest(system1.id),
                std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::ok, resultCode);

        ASSERT_EQ(system1Servers.size(), resolveResponse.hostNames.size());
        for (size_t i = 0; i < system1Servers.size(); ++i)
        {
            const auto it = std::find(
                resolveResponse.hostNames.begin(),
                resolveResponse.hostNames.end(),
                system1Servers[i]->serverId() + "." + system1.id);
            ASSERT_NE(it, resolveResponse.hostNames.end());
        }
    }

    // resolving servers
    for (size_t i = 0; i < system1Servers.size(); ++i)
    {
        api::ResolvePeerResponse resolveResponse;
        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, resolveResponse) =
            makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
                &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
                &client(),
                api::ResolvePeerRequest(
                    system1Servers[i]->serverId() + "." + system1.id),
                std::placeholders::_1));

        ASSERT_EQ(api::ResultCode::ok, resultCode);
        ASSERT_EQ(1U, resolveResponse.endpoints.size());
        ASSERT_EQ(
            system1Servers[i]->endpoint().toString(),
            resolveResponse.endpoints.front().toString());
    }
}

TEST_F(Resolve, same_server_name)
{
    using namespace nx::hpm;

    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, QnUuid::createUuid().toSimpleString().toUtf8());
    auto server2 = addServer(system1, server1->serverId());

    //resolving, last added server is chosen
    api::ResolvePeerResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
            &client(),
            api::ResolvePeerRequest(server1->serverId() + "." + system1.id),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::ok, resultCode);
    ASSERT_EQ(1U, resolveResponse.endpoints.size());
    ASSERT_EQ(
        server2->endpoint().toString(),
        resolveResponse.endpoints.front().toString());
}

TEST_F(Resolve, unkown_domain)
{
    using namespace nx::hpm;

    //resolving unknown system
    api::ResolveDomainResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolveDomainResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolveDomain,
            &client(),
            api::ResolveDomainRequest(nx::utils::generateRandomName(16)),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::notFound, resultCode);
}

TEST_F(Resolve, unkown_host)
{
    using namespace nx::hpm;

    const auto system1 = addRandomSystem();

    //resolving unknown system
    api::ResolvePeerResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
            &client(),
            api::ResolvePeerRequest(system1.id),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::notFound, resultCode);
}

TEST_F(Resolve, resolve_by_system_name)
{
    using namespace nx::hpm;

    const auto system1 = addRandomSystem();

    //emulating local mediaserver
    MediaServerEmulator mserverEmulator(stunUdpEndpoint(), stunTcpEndpoint(), system1);
    ASSERT_TRUE(mserverEmulator.start());
    ASSERT_EQ(api::ResultCode::ok, mserverEmulator.bind());

    //resolving
    api::ResolvePeerResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
            &client(),
            api::ResolvePeerRequest(mserverEmulator.serverId() + "." + system1.id),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::ok, resultCode);
    ASSERT_EQ(1U, resolveResponse.endpoints.size());
    ASSERT_EQ(
        mserverEmulator.endpoint().toString(),
        resolveResponse.endpoints.front().toString());

    //resolve by system name is supported!
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
            &client(),
            api::ResolvePeerRequest(system1.id),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::ok, resultCode);
    ASSERT_EQ(1U, resolveResponse.endpoints.size());
    ASSERT_EQ(
        mserverEmulator.endpoint().toString(),
        resolveResponse.endpoints.front().toString());
}

} // namespace test
} // namespace hpm
} // namespace nx
