/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <utils/common/sync_call.h>

#include "mediaserver_emulator.h"
#include "mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, resolve_unkownHost)
{
    using namespace nx::network::cloud;

    startAndWaitUntilStarted();

    const std::shared_ptr<MediatorClientConnection> client = clientConnection();

    const auto system1 = addRandomSystem();

    //resolving unknown system
    api::ResolveResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &MediatorClientConnection::resolve,
            client.get(),
            api::ResolveRequest(system1.id),
            std::placeholders::_1));

    ASSERT_EQ(api::ResultCode::notFound, resultCode);

    client->pleaseStopSync();
}

TEST_F(MediatorFunctionalTest, resolve_generic)
{
    using namespace nx::network::cloud;

    startAndWaitUntilStarted();

    const std::shared_ptr<MediatorClientConnection> client = clientConnection();

    const auto system1 = addRandomSystem();

    //emulating local mediaserver
    MediaServerEmulator mserverEmulator(endpoint(), system1);
    ASSERT_TRUE(mserverEmulator.start());

    //resolving 
    api::ResolveResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &MediatorClientConnection::resolve,
            client.get(),
            api::ResolveRequest(mserverEmulator.serverID() + "." + system1.id),
            std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::ok, resultCode);

    //resolve by system name is forbidden
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &MediatorClientConnection::resolve,
            client.get(),
            api::ResolveRequest(system1.id),
            std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::notFound, resultCode);

    client->pleaseStopSync();
}

TEST_F(MediatorFunctionalTest, resolve_multipleServers)
{
    //TODO #ak adding multiple server to a system, checking that each very server is found

#if 0
    using namespace nx::network::cloud;

    startAndWaitUntilStarted();

    const std::shared_ptr<MediatorClientConnection> client = clientConnection();

    const auto system1 = addRandomSystem();

    //emulating local mediaserver
    MediaServerEmulator mserverEmulator(endpoint(), system1);
    ASSERT_TRUE(mserverEmulator.start());

    //resolving 
    api::ResolveResponse resolveResponse;
    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &nx::cc::MediatorClientConnection::resolve,
            client.get(),
            ResolveRequest(mserverEmulator.serverID() + "." + system1.id),
            std::placeholders::_1));
    ASSERT_EQ(ResultCode::ok, resultCode);

    //resolve by system name is forbidden
    std::tie(resultCode, resolveResponse) = makeSyncCall<api::ResultCode, api::ResolveResponse>(
        std::bind(
            &nx::cc::MediatorClientConnection::resolve,
            client.get(),
            api::ResolveRequest(system1.id),
            std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::notFound, resultCode);

    client->pleaseStopSync();
#endif
}

} // namespace test
} // namespace hpm
} // namespase nx
