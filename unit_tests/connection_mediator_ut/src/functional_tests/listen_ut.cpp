/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/socket_global.h>

#include <utils/common/string.h>
#include <utils/common/sync_call.h>

#include <listening_peer_pool.h>
#include <test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

class ListeningPeer
:
    public MediatorFunctionalTest
{
};

TEST_F(ListeningPeer, connection_override)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();
    auto server1 = addRandomServer(system1);
    ASSERT_NE(nullptr, server1);
    auto server2 = addServer(system1, server1->serverId());
    ASSERT_NE(nullptr, server2);

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server1->listen());
    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server2->listen());

    //TODO #ak checking that server2 connection has overridden server1 
        //since both servers have same server id

    auto dataLocker = nx::hpm::ListeningPeerPool::instance()->
        findAndLockPeerDataByHostName(server1->serverId()+"."+system1.id);
    ASSERT_TRUE(static_cast<bool>(dataLocker));
    auto strongConnectionRef = dataLocker->value().peerConnection.lock();
    ASSERT_NE(nullptr, strongConnectionRef);
    ASSERT_EQ(
        server2->mediatorConnectionLocalAddress(),
        strongConnectionRef->getSourceAddress());
    dataLocker.reset();

    client->pleaseStopSync();
}

TEST_F(ListeningPeer, unknown_system_credentials)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();
    auto server1 = addRandomServer(system1);
    ASSERT_NE(nullptr, server1);
    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server1->listen());

    auto system2 = addRandomSystem();
    system2.authKey.clear();    //making credentials invalid
    auto server2 = addRandomServerNotRegisteredOnMediator(system2);
    ASSERT_NE(nullptr, server2);
    ASSERT_EQ(nx::hpm::api::ResultCode::notAuthorized, server2->registerOnMediator());
    ASSERT_EQ(nx::hpm::api::ResultCode::notAuthorized, server2->listen());

    client->pleaseStopSync();
}

TEST_F(ListeningPeer, peer_disconnect)
{
    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto system1 = addRandomSystem();
    auto server1 = addRandomServer(system1);
    ASSERT_NE(nullptr, server1);
    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server1->listen());

    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::ok;
    data::ListeningPeersBySystem listeningPeers;
    std::tie(statusCode, listeningPeers) = getListeningPeers();
    ASSERT_EQ(nx_http::StatusCode::ok, statusCode);

    ASSERT_EQ(1, listeningPeers.systems.size());

    server1.reset();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::tie(statusCode, listeningPeers) = getListeningPeers();
    ASSERT_EQ(nx_http::StatusCode::ok, statusCode);
    ASSERT_TRUE(listeningPeers.systems.empty());
}

} // namespace test
} // namespace hpm
} // namespase nx
