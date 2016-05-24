/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <utils/common/string.h>
#include <utils/common/sync_call.h>

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

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

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
        ASSERT_TRUE(std::find(
            resolveResponse.endpoints.begin(),
            resolveResponse.endpoints.end(),
            system1Servers[i]->endpoint()) != resolveResponse.endpoints.end());
    }

    client->pleaseStopSync();
}

TEST_F(MediatorFunctionalTest, resolve_same_server_name)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, generateRandomName(16));
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
    ASSERT_TRUE(std::find(
        resolveResponse.endpoints.begin(),
        resolveResponse.endpoints.end(),
        server2->endpoint()) != resolveResponse.endpoints.end());
    ASSERT_TRUE(std::find(
        resolveResponse.endpoints.begin(),
        resolveResponse.endpoints.end(),
        server1->endpoint()) == resolveResponse.endpoints.end());

    client->pleaseStopSync();
}

TEST_F(MediatorFunctionalTest, resolve_unkownDomain)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    //resolving unknown system
    api::ResolveDomainResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolveDomainResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolveDomain,
            client.get(),
            api::ResolveDomainRequest(generateRandomName(16)),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::notFound, resultCode);

    client->pleaseStopSync();
}

TEST_F(MediatorFunctionalTest, resolve_unkownHost)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

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

    client->pleaseStopSync();
}

TEST_F(MediatorFunctionalTest, resolve_by_system_name)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();

    //emulating local mediaserver
    MediaServerEmulator mserverEmulator(stunEndpoint(), system1);
    ASSERT_TRUE(mserverEmulator.start());
    ASSERT_EQ(api::ResultCode::ok, mserverEmulator.registerOnMediator());

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
    ASSERT_TRUE(std::find(
        resolveResponse.endpoints.begin(),
        resolveResponse.endpoints.end(),
        mserverEmulator.endpoint()) != resolveResponse.endpoints.end());

    //resolve by system name is supported!
    std::tie(resultCode, resolveResponse) =
        makeSyncCall<api::ResultCode, api::ResolvePeerResponse>(std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolvePeer,
            client.get(),
            api::ResolvePeerRequest(system1.id),
            std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::ok, resultCode);
    ASSERT_TRUE(std::find(
        resolveResponse.endpoints.begin(),
        resolveResponse.endpoints.end(),
        mserverEmulator.endpoint()) != resolveResponse.endpoints.end());

    client->pleaseStopSync();
}

} // namespace test
} // namespace hpm
} // namespase nx
