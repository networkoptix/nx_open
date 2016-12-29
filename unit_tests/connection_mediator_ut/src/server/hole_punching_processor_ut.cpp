/**********************************************************
* Jan 19, 2016
* akolesnikov
***********************************************************/

#include <future>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

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

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1, boost::none, hpm::ServerTweak::noBindEndpoint);
    ASSERT_NE(nullptr, server1);

    static const std::vector<std::list<SocketAddress>> kTestCases =
    {
        {}, // no public addresses
        { server1->endpoint() },
        { server1->endpoint(), SocketAddress("12.34.56.78:12345") },
    };

    for (const auto& directTcpAddresses : kTestCases)
    {
        if (directTcpAddresses.size())
            server1->updateTcpAddresses(directTcpAddresses);

        boost::optional<api::ConnectionRequestedEvent> connectionRequestedEventData;
        server1->setOnConnectionRequestedHandler(
            [&connectionRequestedEventData](api::ConnectionRequestedEvent data)
                -> MediaServerEmulator::ActionToTake
            {
                connectionRequestedEventData = std::move(data);
                return MediaServerEmulator::ActionToTake::proceedWithConnection;
            });

        //TODO #ak #msvc2015 use future/promise
        std::promise<api::ResultCode> connectionAckResultPromise;
        server1->setConnectionAckResponseHandler(
            [&connectionAckResultPromise](api::ResultCode resultCode)
                -> MediaServerEmulator::ActionToTake
            {
                connectionAckResultPromise.set_value(resultCode);
                return MediaServerEmulator::ActionToTake::ignoreIndication;
            });

        const auto listenResult = server1->listen();
        ASSERT_EQ(api::ResultCode::ok, listenResult.first);
        ASSERT_EQ(KeepAliveOptions(10, 10, 3), listenResult.second.tcpConnectionKeepAlive);

        //requesting connect to the server
        nx::hpm::api::MediatorClientUdpConnection udpClient(stunEndpoint());

        std::promise<api::ResultCode> connectResultPromise;

        api::ConnectResponse connectResponseData;
        auto connectCompletionHandler =
            [&connectResultPromise, &connectResponseData](
                stun::TransportHeader /*stunTransportHeader*/,
                api::ResultCode resultCode,
                api::ConnectResponse responseData)
            {
                connectResponseData = std::move(responseData);
                connectResultPromise.set_value(resultCode);
            };
        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerId = QnUuid::createUuid().toByteArray();
        connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.destinationHostName = server1->serverId() + "." + system1.id;
        udpClient.connect(
            connectRequest,
            connectCompletionHandler);

        //waiting for connect response and checking server UDP endpoint in response
        ASSERT_EQ(api::ResultCode::ok, connectResultPromise.get_future().get());
        ASSERT_FALSE(connectResponseData.udpEndpointList.empty());
        ASSERT_EQ(
            lm("%1").container(directTcpAddresses).toStdString(),
            lm("%1").container(connectResponseData.forwardedTcpEndpointList).toStdString());
        ASSERT_EQ(
            server1->udpHolePunchingEndpoint().port,
            connectResponseData.udpEndpointList.front().port);

        //checking server has received connectionRequested indication
        ASSERT_TRUE(static_cast<bool>(connectionRequestedEventData));
        ASSERT_EQ(
            connectRequest.originatingPeerId,
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
                    &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                    &udpClient,
                    std::move(connectionResult),
                    std::placeholders::_1));

        //waiting for connectionAck response to be received by server
        ASSERT_EQ(api::ResultCode::ok, connectionAckResultPromise.get_future().get());

        //testing that mediator has cleaned up session data
        std::tie(resultCode) =
            makeSyncCall<api::ResultCode>(
                std::bind(
                    &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                    &udpClient,
                    std::move(connectionResult),
                    std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::notFound, resultCode);

        udpClient.pleaseStopSync();
    }
}

TEST_F(HolePunchingProcessor, server_failure)
{
    const std::chrono::milliseconds kMaxConnectResponseWaitTimeout(15000);

    using namespace nx::hpm;

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1, boost::none);

    typedef MediaServerEmulator::ActionToTake MsAction;
    static const std::map<MsAction, api::ResultCode> kTestCases =
    {
        { MsAction::ignoreIndication, api::ResultCode::noReplyFromServer },
        { MsAction::closeConnectionToMediator, api::ResultCode::notFound },
    };

    for (const auto& testCase: kTestCases)
    {
        const auto actionToTake = testCase.first;
        const auto expectedResult = testCase.second;

        //TODO #ak #msvc2015 use future/promise
        QnMutex mtx;
        QnWaitCondition waitCond;

        boost::optional<api::ConnectionRequestedEvent> connectionRequestedEventData;
        server1->setOnConnectionRequestedHandler(
            [&connectionRequestedEventData, actionToTake](api::ConnectionRequestedEvent data)
                -> MediaServerEmulator::ActionToTake
        {
            connectionRequestedEventData = std::move(data);
            return actionToTake;
        });

        ASSERT_EQ(api::ResultCode::ok, server1->listen().first);

        //requesting connect to the server 
        nx::hpm::api::MediatorClientUdpConnection udpClient(stunEndpoint());

        boost::optional<api::ResultCode> connectResult;

        api::ConnectResponse connectResponseData;
        auto connectCompletionHandler =
            [&mtx, &waitCond, &connectResult, &connectResponseData](
                stun::TransportHeader /*stunTransportHeader*/,
                api::ResultCode resultCode,
                api::ConnectResponse responseData)
        {
            QnMutexLocker lk(&mtx);
            connectResult = resultCode;
            connectResponseData = std::move(responseData);
            waitCond.wakeAll();
        };
        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerId = QnUuid::createUuid().toByteArray();
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
        ASSERT_EQ(expectedResult, connectResultVal);

        if (actionToTake != MsAction::closeConnectionToMediator)
        {
            //testing that mediator has cleaned up session data
            api::ResultCode resultCode = api::ResultCode::ok;
            api::ConnectionResultRequest connectionResult;
            connectionResult.connectSessionId = connectRequest.connectSessionId;
            connectionResult.resultCode = api::NatTraversalResultCode::udtConnectFailed;
            std::tie(resultCode) =
                makeSyncCall<api::ResultCode>(
                    std::bind(
                        &nx::hpm::api::MediatorClientUdpConnection::send<api::ConnectionResultRequest>,
                        &udpClient,
                        std::move(connectionResult),
                        std::placeholders::_1));
            ASSERT_EQ(api::ResultCode::notFound, resultCode);
        }

        udpClient.pleaseStopSync();
    }
}

TEST_F(HolePunchingProcessor, destruction)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto system1 = addRandomSystem();
    const auto server1 = addRandomServer(system1);

    ASSERT_EQ(api::ResultCode::ok, server1->listen().first);

    for (int i = 0; i < 100; ++i)
    {
        nx::hpm::api::MediatorClientUdpConnection udpClient(stunEndpoint());

        api::ConnectRequest connectRequest;
        connectRequest.originatingPeerId = QnUuid::createUuid().toByteArray();
        connectRequest.connectSessionId = QnUuid::createUuid().toByteArray();
        connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
        connectRequest.destinationHostName = server1->serverId() + "." + system1.id;
        std::promise<void> connectResponsePromise;
        udpClient.connect(
            connectRequest,
            [&connectResponsePromise](
                stun::TransportHeader /*stunTransportHeader*/,
                api::ResultCode /*resultCode*/,
                api::ConnectResponse /*responseData*/)
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
