/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "connector_factory.h"

#include "udp/connector.h"


namespace nx {
namespace network {
namespace cloud {

static ConnectorFactory::FactoryFunc factoryFunc;

ConnectorFactory::CloudConnectors 
    ConnectorFactory::createAllCloudConnectors(const AddressEntry& address)
{
    if (factoryFunc)
        return factoryFunc(address);

    CloudConnectors connectors;
    connectors.emplace(
        CloudConnectType::kUdtHp,
        std::make_unique<udp::TunnelConnector>(
            address));
    return connectors;
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
