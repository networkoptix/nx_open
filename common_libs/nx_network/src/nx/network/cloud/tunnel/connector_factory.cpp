/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "connector_factory.h"

#include "cross_nat_connector.h"
#include "nx/network/socket_global.h"
#include "udp/connector.h"


namespace nx {
namespace network {
namespace cloud {

ConnectorFactory::CloudConnectors 
    ConnectorFactory::createAllCloudConnectors(const AddressEntry& address)
{
    NX_CRITICAL(false);

    CloudConnectors connectors;
    //auto udpHolePunchingConnector = std::make_unique<udp::TunnelConnector>(address);
    //connectors.emplace(
    //    CloudConnectType::kUdtHp,
    //    std::move(udpHolePunchingConnector));
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
