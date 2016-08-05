/**********************************************************
* Jul 26, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tcp/direct_endpoint_connector.h>

#include "cross_nat_connector_test.h"


namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

using nx::hpm::MediaServerEmulator;

class TcpTunnelConnector
:
    public cloud::test::TunnelConnector
{
public:
    TcpTunnelConnector()
    {
        //disabling udp hole punching and enabling tcp port forwarding
        ConnectorFactory::setEnabledCloudConnectMask(
            (int)CloudConnectType::forwardedTcpPort);
    }

    ~TcpTunnelConnector()
    {
        ConnectorFactory::setEnabledCloudConnectMask(
            (int)CloudConnectType::all);
    }
};

TEST_F(TcpTunnelConnector, general)
{
    generalTest();
}

TEST_F(TcpTunnelConnector, cancellation)
{
    cancellationTest();
}

}   //namespace test
}   //namespace tcp
}   //namespace cloud
}   //namespace network
}   //namespace nx
