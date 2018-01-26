#include "connector_factory.h"

#include "cross_nat_connector.h"
#include "nx/network/socket_global.h"
#include "udp/connector.h"
#include "tcp/direct_endpoint_connector.h"
#include "relay/relay_connector.h"

namespace nx {
namespace network {
namespace cloud {

static int s_cloudConnectTypeMask = (int)CloudConnectType::all;

using namespace std::placeholders;

ConnectorFactory::ConnectorFactory():
    base_type(std::bind(&ConnectorFactory::defaultFactoryFunction, this, _1, _2, _3, _4))
{
}

ConnectorFactory& ConnectorFactory::instance()
{
    static ConnectorFactory instance;
    return instance;
}

CloudConnectors ConnectorFactory::defaultFactoryFunction(
    const AddressEntry& targetAddress,
    const nx::String& connectSessionId,
    const hpm::api::ConnectResponse& response,
    std::unique_ptr<UDPSocket> udpSocket)
{
    CloudConnectors connectors;

    if (((s_cloudConnectTypeMask & (int)CloudConnectType::udpHp) > 0) &&
        (udpSocket && !response.udpEndpointList.empty()))
    {
        TunnelConnectorContext context;
        context.connector = std::make_unique<udp::TunnelConnector>(
            targetAddress,
            connectSessionId,
            std::move(udpSocket));
        context.startDelay = response.params.udpHolePunchingStartDelay;
        connectors.emplace_back(std::move(context));
    }

    if (((s_cloudConnectTypeMask & (int)CloudConnectType::forwardedTcpPort) > 0) &&
        !response.forwardedTcpEndpointList.empty())
    {
        TunnelConnectorContext context;
        context.connector = std::make_unique<tcp::DirectEndpointConnector>(
            targetAddress, connectSessionId);
        context.startDelay = response.params.directTcpConnectStartDelay;
        connectors.emplace_back(std::move(context));
    }

    if (((s_cloudConnectTypeMask & (int)CloudConnectType::proxy) > 0) &&
        response.trafficRelayUrl)
    {
        TunnelConnectorContext context;
        context.connector = std::make_unique<relay::Connector>(
            QUrl(*response.trafficRelayUrl), targetAddress, connectSessionId);
        context.startDelay = response.params.trafficRelayingStartDelay;
        connectors.emplace_back(std::move(context));
    }

    return connectors;
}

void ConnectorFactory::setEnabledCloudConnectMask(int cloudConnectTypeMask)
{
    s_cloudConnectTypeMask = cloudConnectTypeMask;
}

int ConnectorFactory::getEnabledCloudConnectMask()
{
    return s_cloudConnectTypeMask;
}

//-------------------------------------------------------------------------------------------------

CrossNatConnectorFactory::CrossNatConnectorFactory():
    base_type(std::bind(&CrossNatConnectorFactory::defaultFactoryFunction, this, _1))
{
}

CrossNatConnectorFactory& CrossNatConnectorFactory::instance()
{
    static CrossNatConnectorFactory instance;
    return instance;
}

std::unique_ptr<AbstractCrossNatConnector> CrossNatConnectorFactory::defaultFactoryFunction(
    const AddressEntry& address)
{
    return std::make_unique<CrossNatConnector>(address);
}

} // namespace cloud
} // namespace network
} // namespace nx
