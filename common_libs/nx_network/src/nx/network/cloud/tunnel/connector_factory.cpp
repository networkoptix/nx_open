/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "connector_factory.h"

#include "nx/network/socket_global.h"
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
    auto udpHolePunchingConnector = std::make_unique<udp::TunnelConnector>(address);
    const auto originatingHostAddressReplacement = 
        SocketGlobals::cloudConnectSettings().originatingHostAddressReplacement();
    if (originatingHostAddressReplacement)
        udpHolePunchingConnector->replaceOriginatingHostAddress(
            *originatingHostAddressReplacement);
    connectors.emplace(
        CloudConnectType::kUdtHp,
        std::move(udpHolePunchingConnector));
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
