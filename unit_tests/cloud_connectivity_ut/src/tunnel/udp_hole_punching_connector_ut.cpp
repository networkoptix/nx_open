/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include <future>

#include <boost/optional.hpp>
#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/tunnel/udp_hole_punching_connector.h>
#include <libconnection_mediator/src/test_support/mediator_functional_test.h>


namespace nx {
namespace network {
namespace cloud {
namespace test {

class UdpHolePunchingTunnelConnector
:
    public ::testing::Test
{
public:
    ~UdpHolePunchingTunnelConnector()
    {
        if (m_oldFactoryFunc)
            ConnectorFactory::setFactoryFunc(std::move(*m_oldFactoryFunc));
    }

    void setConnectorFactoryFunc(ConnectorFactory::FactoryFunc newFactoryFunc)
    {
        auto oldFunc = ConnectorFactory::setFactoryFunc(std::move(newFactoryFunc));
        if (!m_oldFactoryFunc)
            m_oldFactoryFunc = oldFunc;
    }

    const hpm::MediatorFunctionalTest& mediator() const
    {
        return m_mediator;
    }

    hpm::MediatorFunctionalTest& mediator()
    {
        return m_mediator;
    }

private:
    boost::optional<ConnectorFactory::FactoryFunc> m_oldFactoryFunc;
    hpm::MediatorFunctionalTest m_mediator;
};

using nx::hpm::MediaServerEmulator;

TEST_F(UdpHolePunchingTunnelConnector, general)
{
    //starting mediator
    ASSERT_TRUE(mediator().startAndWaitUntilStarted());

    //starting mediaserver emulator with specified host name
    const auto system1 = mediator().addRandomSystem();
    const auto server1 = mediator().addRandomServer(system1);
    server1->setConnectionAckResponseHandler(
        [](nx::hpm::api::ResultCode resultCode)
            -> MediaServerEmulator::ActionToTake
        {
            return resultCode == nx::hpm::api::ResultCode::ok
                ? MediaServerEmulator::ActionToTake::proceedWithConnection
                : MediaServerEmulator::ActionToTake::ignoreIndication;
        });

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server1->listen());

    std::promise<std::pair<SystemError::ErrorCode /*errorCode*/,
        std::unique_ptr<AbstractOutgoingTunnelConnection >>> connectedPromise;
    cloud::UdpHolePunchingTunnelConnector connector(
        SocketAddress((server1->serverId() + "." + system1.id).constData()));
    connector.connect(
        std::chrono::milliseconds(0),   //no timeout
        [&connectedPromise](
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
        {
            connectedPromise.set_value(
                std::make_pair(errorCode, std::move(connection)));
        });

    const auto connectResult = connectedPromise.get_future().get();

    ASSERT_EQ(SystemError::timedOut, connectResult.first);
    ASSERT_EQ(nullptr, connectResult.second);

    connector.pleaseStopSync();
}

//TEST(UdpHolePunchingTunnelConnector, timeout)
//TEST(UdpHolePunchingTunnelConnector, cancellation)

}   //namespace test
}   //namespace cloud
}   //namespace network
}   //namespace nx
