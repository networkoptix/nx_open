/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "connector_factory.h"

#include "cross_nat_connector.h"
#include "nx/network/socket_global.h"
#include "udp/connector.h"
#include "tcp/forwarded_endpoint_connector.h"


namespace nx {
namespace network {
namespace cloud {

ConnectorFactory::CloudConnectors ConnectorFactory::createCloudConnectors(
    const AddressEntry& targetAddress,
    const nx::String& connectSessionId,
    const hpm::api::ConnectResponse& response,
    std::unique_ptr<UDPSocket> udpSocket)
{
    CloudConnectors connectors;

    connectors.emplace_back(
        std::make_unique<udp::TunnelConnector>(
            targetAddress,
            connectSessionId,
            std::move(udpSocket)));

    if (!response.forwardedTcpEndpointList.empty())
    {
        connectors.emplace_back(
            std::make_unique<tcp::ForwardedEndpointConnector>(
                targetAddress, connectSessionId));
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

} // namespace cloud
} // namespace network
} // namespace nx
