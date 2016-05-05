/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <map>

#include "abstract_tunnel_connector.h"
#include "nx/network/cloud/address_resolver.h"


namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API ConnectorFactory
{
public:
    typedef std::map<
        CloudConnectType,
        std::unique_ptr<AbstractTunnelConnector>> CloudConnectors;
    typedef std::function<CloudConnectors(const AddressEntry& /*targetAddress*/)> FactoryFunc;

    static CloudConnectors createAllCloudConnectors(const AddressEntry& address);
    /** Set */
    static FactoryFunc setFactoryFunc(FactoryFunc newFactoryFunc);
};

} // namespace cloud
} // namespace network
} // namespace nx
