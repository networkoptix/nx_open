/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>

#include <test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, resolve_generic)
{
    //TODO #ak adding multiple server to a system, checking that each very server is found

    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto client = clientConnection();
    auto clientGuard = makeScopeGuard([&client]() { client->pleaseStopSync(); });

    const auto system1 = addRandomSystem();
    auto system1Servers = addRandomServers(system1, 2);

    // resolve system
    {
        api::ResolveDomainResponse resolveResponse;
        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, resolveResponse) =
            makeSyncCall<api::ResultCode, api::ResolveDomainResponse>(std::bind(
                &nx::hpm::api::MediatorClientTcpConnection::resolveDomain,
                client.get(),
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
                client.get(),
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

TEST_F(MediatorFunctionalTest, resolve_same_server_name)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto client = clientConnection();
    auto clientGuard = makeScopeGuard([&client]() { client->pleaseStopSync(); });

    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, QnUuid::createUuid().toSimpleString().toUtf8());
    auto server2 = addServer(system1, server1->serverId());

    //resolving, last added server is chosen
    api::ResolvePeerResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
            client.get(),
            api::ResolvePeerRequest(server1->serverId() + "." + system1.id),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::ok, resultCode);
    ASSERT_EQ(1U, resolveResponse.endpoints.size());
    ASSERT_EQ(
        server2->endpoint().toString(),
        resolveResponse.endpoints.front().toString());
}

TEST_F(MediatorFunctionalTest, resolve_unkownDomain)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto client = clientConnection();
    auto clientGuard = makeScopeGuard([&client]() { client->pleaseStopSync(); });

    //resolving unknown system
    api::ResolveDomainResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolveDomainResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolveDomain,
            client.get(),
            api::ResolveDomainRequest(nx::utils::generateRandomName(16)),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::notFound, resultCode);
}

TEST_F(MediatorFunctionalTest, resolve_unkownHost)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto client = clientConnection();
    auto clientGuard = makeScopeGuard([&client]() { client->pleaseStopSync(); });

    const auto system1 = addRandomSystem();

    //resolving unknown system
    api::ResolvePeerResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
            client.get(),
            api::ResolvePeerRequest(system1.id),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::notFound, resultCode);
}

TEST_F(MediatorFunctionalTest, resolve_by_system_name)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto client = clientConnection();
    auto clientGuard = makeScopeGuard([&client]() { client->pleaseStopSync(); });

    const auto system1 = addRandomSystem();

    //emulating local mediaserver
    MediaServerEmulator mserverEmulator(stunEndpoint(), system1);
    ASSERT_TRUE(mserverEmulator.start());
    ASSERT_EQ(api::ResultCode::ok, mserverEmulator.bind());

    //resolving
    api::ResolvePeerResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
            client.get(),
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
            client.get(),
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
