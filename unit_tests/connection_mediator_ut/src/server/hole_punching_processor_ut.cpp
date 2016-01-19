/**********************************************************
* Jan 19, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <utils/common/sync_call.h>

#include "functional_tests/mediaserver_emulator.h"
#include "functional_tests/mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, HolePunchingProcessor_generic)
{
    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1);

    ASSERT_EQ(api::ResultCode::ok, server1->listen());

    //TODO requesting connect to the server 
    nx::network::cloud::MediatorClientUdpConnection udpClient(
        server1->endpoint());
    std::function<void(
        nx::hpm::api::ResultCode,
        nx::hpm::api::ConnectResponse)> completionHandler;
    udpClient.connect(
        nx::hpm::api::ConnectRequest(),
        std::move(completionHandler));

    //TODO checking server has received connectionRequested indication
    //TODO checking server has received connectionAck response
    //TODO waiting for connect response and checking server UDP endpoint in response

    udpClient.pleaseStopSync();
}

}   //test
}   //hpm
}   //nx
