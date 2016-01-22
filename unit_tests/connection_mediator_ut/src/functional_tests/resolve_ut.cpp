/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <utils/common/string.h>
#include <utils/common/sync_call.h>

#include "mediaserver_emulator.h"
#include "mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, resolve_generic)
{
    //TODO #ak adding multiple server to a system, checking that each very server is found

    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();
    auto system1Servers = addRandomServers(system1, 2);

    //resolving 
    for (int i = 0; i < system1Servers.size(); ++i)
    {
        api::ResolveResponse resolveResponse;
        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
            std::bind(
                &nx::hpm::api::MediatorClientTcpConnection::resolve,
                client.get(),
                api::ResolveRequest(system1Servers[i]->serverId() + "." + system1.id),
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

    startAndWaitUntilStarted();

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, generateRandomName(16));
    auto server2 = addServer(system1, server1->serverId());

    //resolving, last added server is chosen
    api::ResolveResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolve,
            client.get(),
            api::ResolveRequest(server1->serverId() + "." + system1.id),
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

TEST_F(MediatorFunctionalTest, resolve_unkownHost)
{
    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();

    //resolving unknown system
    api::ResolveResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolve,
            client.get(),
            api::ResolveRequest(system1.id),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::notFound, resultCode);

    client->pleaseStopSync();
}

TEST_F(MediatorFunctionalTest, resolve_forbidden_by_system_name)
{
    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();

    //emulating local mediaserver
    MediaServerEmulator mserverEmulator(endpoint(), system1);
    ASSERT_TRUE(mserverEmulator.start());

    //resolving 
    api::ResolveResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolve,
            client.get(),
            api::ResolveRequest(mserverEmulator.serverId() + "." + system1.id),
            std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::ok, resultCode);
    ASSERT_TRUE(std::find(
        resolveResponse.endpoints.begin(),
        resolveResponse.endpoints.end(),
        mserverEmulator.endpoint()) != resolveResponse.endpoints.end());

    //resolve by system name is forbidden
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &nx::hpm::api::MediatorClientTcpConnection::resolve,
            client.get(),
            api::ResolveRequest(system1.id),
            std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::notFound, resultCode);

    client->pleaseStopSync();
}

} // namespace test
} // namespace hpm
} // namespase nx
