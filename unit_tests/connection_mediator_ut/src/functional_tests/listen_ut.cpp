/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <utils/common/string.h>
#include <utils/common/sync_call.h>

#include <listening_peer_pool.h>

#include "mediaserver_emulator.h"
#include "mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, listen)
{
    //TODO #ak adding multiple server to a system, checking that each very server is found

    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
        client = clientConnection();

    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, nx::String());
    auto server2 = addServer(system1, server1->serverId());

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

} // namespace test
} // namespace hpm
} // namespase nx
