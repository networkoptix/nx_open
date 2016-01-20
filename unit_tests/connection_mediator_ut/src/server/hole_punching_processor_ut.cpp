/**********************************************************
* Jan 19, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <utils/common/sync_call.h>

#include "functional_tests/mediaserver_emulator.h"
#include "functional_tests/mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

TEST_F(MediatorFunctionalTest, HolePunchingProcessor_generic)
{
    const std::chrono::milliseconds kMaxConnectResponseWaitTimeout(15000);

    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1);

    boost::optional<api::ConnectionRequestedEvent> connectionRequestedEventData;
    server1->setOnConnectionRequestedHandler(
        [&connectionRequestedEventData](api::ConnectionRequestedEvent data)
        {
            connectionRequestedEventData = std::move(data);
        });

    ASSERT_EQ(api::ResultCode::ok, server1->listen());

    //requesting connect to the server 
    nx::hpm::api::MediatorClientUdpConnection udpClient(endpoint());

    boost::optional<api::ResultCode> connectResult;
    QnMutex mtx;
    QnWaitCondition waitCond;

    api::ConnectResponse connectResponseData;
    auto connectCompletionHandler =
        [&mtx, &waitCond, &connectResult, &connectResponseData](
            api::ResultCode resultCode,
            api::ConnectResponse responseData)
        {
            QnMutexLocker lk(&mtx);
            connectResult = resultCode;
            connectResponseData = std::move(responseData);
            waitCond.wakeAll();
        };
    api::ConnectRequest connectRequest;
    connectRequest.originatingPeerID = QnUuid::createUuid().toByteArray();
    connectRequest.connectSessionID = QnUuid::createUuid().toByteArray();
    connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
    connectRequest.destinationHostName = server1->serverId() + "." + system1.id;
    udpClient.connect(
        connectRequest,
        connectCompletionHandler);

    //waiting for connect response and checking server UDP endpoint in response
    {
        QnMutexLocker lk(&mtx);
        while (!static_cast<bool>(connectResult))
            ASSERT_TRUE(waitCond.wait(lk.mutex(), kMaxConnectResponseWaitTimeout.count()));
    }

    ASSERT_EQ(api::ResultCode::ok, connectResult.get());
    ASSERT_FALSE(connectResponseData.udpEndpointList.empty());
    ASSERT_EQ(
        server1->udpHolePunchingEndpoint(),
        connectResponseData.udpEndpointList.front());

    //checking server has received connectionRequested indication
    ASSERT_TRUE(static_cast<bool>(connectionRequestedEventData));
    ASSERT_EQ(
        connectRequest.originatingPeerID,
        connectionRequestedEventData->originatingPeerID);
    ASSERT_EQ(1, connectionRequestedEventData->udpEndpointList.size());
    ASSERT_EQ(
        udpClient.localAddress(),
        connectionRequestedEventData->udpEndpointList.front());

    api::ResultCode resultCode = api::ResultCode::ok;
    api::ConnectionResultRequest connectionResult;
    connectionResult.connectSessionID = connectRequest.connectSessionID;
    connectionResult.connectionSucceeded = true;
    std::tie(resultCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::hpm::api::MediatorClientUdpConnection::connectionResult,
                &udpClient,
                std::move(connectionResult),
                std::placeholders::_1));

    //TODO #ak waiting for connectionAck response to be received by server

    udpClient.pleaseStopSync();
}

}   //test
}   //hpm
}   //nx
