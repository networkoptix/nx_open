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

ConnectorFactory::CloudConnectors ConnectorFactory::createCloudConnectors(
    const AddressEntry& targetAddress,
    const nx::String& connectSessionId,
    const hpm::api::ConnectResponse& response,
    std::unique_ptr<UDPSocket> udpSocket)
{
    CloudConnectors connectors;

    if (((s_cloudConnectTypeMask & (int)CloudConnectType::udpHp) > 0) &&
        (udpSocket || !response.udpEndpointList.empty()))
    {
        connectors.emplace_back(
            std::make_unique<udp::TunnelConnector>(
                targetAddress,
                connectSessionId,
                std::move(udpSocket)));
    }

    if (((s_cloudConnectTypeMask & (int)CloudConnectType::forwardedTcpPort) > 0) &&
        !response.forwardedTcpEndpointList.empty())
    {
        connectors.emplace_back(
            std::make_unique<tcp::DirectEndpointConnector>(
                targetAddress, connectSessionId));
    }

    if (((s_cloudConnectTypeMask & (int)CloudConnectType::proxy) > 0) &&
        !response.trafficRelayEndpointList.empty())
    {
        connectors.emplace_back(
            std::make_unique<relay::Connector>(
                response.trafficRelayEndpointList.front(), targetAddress, connectSessionId));
    }

    return connectors;
}

static ConnectorFactory::FactoryFunc factoryFunc;

std::unique_ptr<AbstractCrossNatConnector> 
    ConnectorFactory::createCrossNatConnector(const AddressEntry& address)
{
    if (factoryFunc)
        return factoryFunc(address);

    return std::make_unique<CrossNatConnector>(address);
}

ConnectorFactory::FactoryFunc 
    ConnectorFactory::setFactoryFunc(FactoryFunc newFactoryFunc)
{
    auto bak = std::move(factoryFunc);
    factoryFunc = std::move(newFactoryFunc);
    return bak;
}

void ConnectorFactory::setEnabledCloudConnectMask(int cloudConnectTypeMask)
{
    s_cloudConnectTypeMask = cloudConnectTypeMask;
}

int ConnectorFactory::getEnabledCloudConnectMask()
{
    return s_cloudConnectTypeMask;
}

} // namespace cloud
} // namespace network
} // namespace nx
