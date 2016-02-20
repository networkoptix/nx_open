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

#include <test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"


namespace nx {
namespace hpm {
namespace test {

typedef MediatorFunctionalTest HolePunchingProcessor;

TEST_F(HolePunchingProcessor, generic_tests)
{
    const std::chrono::milliseconds kMaxConnectResponseWaitTimeout(15000);

    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1);
    ASSERT_NE(nullptr, server1);

    //TODO #ak #msvc2015 use future/promise
    QnMutex mtx;
    QnWaitCondition waitCond;

    boost::optional<api::ConnectionRequestedEvent> connectionRequestedEventData;
    server1->setOnConnectionRequestedHandler(
        [&connectionRequestedEventData](api::ConnectionRequestedEvent data)
            -> MediaServerEmulator::ActionToTake
        {
            connectionRequestedEventData = std::move(data);
            return MediaServerEmulator::ActionToTake::proceedWithConnection;
        });

    boost::optional<api::ResultCode> connectionAckResult;
    server1->setConnectionAckResponseHandler(
        [&mtx, &waitCond, &connectionAckResult](api::ResultCode resultCode)
            -> MediaServerEmulator::ActionToTake
        {
            QnMutexLocker lk(&mtx);
            connectionAckResult = resultCode;
            waitCond.wakeAll();
            return MediaServerEmulator::ActionToTake::ignoreIndication;
        });

    ASSERT_EQ(api::ResultCode::ok, server1->listen());

    //requesting connect to the server 
    nx::hpm::api::MediatorClientUdpConnection udpClient(endpoint());

    boost::optional<api::ResultCode> connectResult;

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
    connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
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
        server1->udpHolePunchingEndpoint().port,
        connectResponseData.udpEndpointList.front().port);

    //checking server has received connectionRequested indication
    ASSERT_TRUE(static_cast<bool>(connectionRequestedEventData));
    ASSERT_EQ(
        connectRequest.originatingPeerID,
        connectionRequestedEventData->originatingPeerID);
    ASSERT_EQ(1, connectionRequestedEventData->udpEndpointList.size());
    ASSERT_EQ(
        udpClient.localAddress().port,
        connectionRequestedEventData->udpEndpointList.front().port);    //not comparing interface since it may be 0.0.0.0 for server

    api::ResultCode resultCode = api::ResultCode::ok;
    api::ConnectionResultRequest connectionResult;
    connectionResult.connectSessionId = connectRequest.connectSessionId;
    std::tie(resultCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::hpm::api::MediatorClientUdpConnection::connectionResult,
                &udpClient,
                std::move(connectionResult),
                std::placeholders::_1));

    //waiting for connectionAck response to be received by server
    {
        QnMutexLocker lk(&mtx);
        while (!static_cast<bool>(connectionAckResult))
            ASSERT_TRUE(waitCond.wait(lk.mutex(), kMaxConnectResponseWaitTimeout.count()));
    }
    ASSERT_TRUE(static_cast<bool>(connectionAckResult));
    ASSERT_EQ(api::ResultCode::ok, connectionAckResult.get());

    //testing that mediator has cleaned up session data
    std::tie(resultCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::hpm::api::MediatorClientUdpConnection::connectionResult,
                &udpClient,
                std::move(connectionResult),
                std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::notFound, resultCode);

    udpClient.pleaseStopSync();
}

TEST_F(HolePunchingProcessor, server_failure)
{
    const std::chrono::milliseconds kMaxConnectResponseWaitTimeout(15000);

    using namespace nx::hpm;

    startAndWaitUntilStarted();

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1);

    for (int i = 0; i < 2; ++i)
    {
        //TODO #ak #msvc2015 use future/promise
        QnMutex mtx;
        QnWaitCondition waitCond;

        const MediaServerEmulator::ActionToTake actionToTake =
            i == 0
            ? MediaServerEmulator::ActionToTake::ignoreIndication
            : MediaServerEmulator::ActionToTake::closeConnectionToMediator;

        boost::optional<api::ConnectionRequestedEvent> connectionRequestedEventData;
        server1->setOnConnectionRequestedHandler(
            [&connectionRequestedEventData, actionToTake](api::ConnectionRequestedEvent data)
                -> MediaServerEmulator::ActionToTake
        {
            connectionRequestedEventData = std::move(data);
            return actionToTake;
        });

        ASSERT_EQ(api::ResultCode::ok, server1->listen());

        //requesting connect to the server 
        nx::hpm::api::MediatorClientUdpConnection udpClient(endpoint());

        boost::optional<api::ResultCode> connectResult;

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
        connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
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

        const auto connectResultVal = connectResult.get();
        ASSERT_EQ(api::ResultCode::noReplyFromServer, connectResultVal);

        //testing that mediator has cleaned up session data
        api::ResultCode resultCode = api::ResultCode::ok;
        api::ConnectionResultRequest connectionResult;
        connectionResult.connectSessionId = connectRequest.connectSessionId;
        connectionResult.resultCode = api::UdpHolePunchingResultCode::udtConnectFailed;
        std::tie(resultCode) =
            makeSyncCall<api::ResultCode>(
                std::bind(
                    &nx::hpm::api::MediatorClientUdpConnection::connectionResult,
                    &udpClient,
                    std::move(connectionResult),
                    std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::notFound, resultCode);

        udpClient.pleaseStopSync();
    }
}

TEST_F(HolePunchingProcessor, destruction)
{
    startAndWaitUntilStarted();

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1);

    ASSERT_EQ(api::ResultCode::ok, server1->listen());

    for (int i = 0; i < 100; ++i)
    {
        nx::hpm::api::MediatorClientUdpConnection udpClient(endpoint());

        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerID = QnUuid::createUuid().toByteArray();
        connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.destinationHostName = server1->serverId() + "." + system1.id;
        std::promise<void> connectResponsePromise;
        udpClient.connect(
            connectRequest,
            [&connectResponsePromise](
                api::ResultCode resultCode,
                api::ConnectResponse responseData)
            {
                connectResponsePromise.set_value();
            });
        connectResponsePromise.get_future().wait();

        udpClient.pleaseStopSync();
    }
}

}   //test
}   //hpm
}   //nx
