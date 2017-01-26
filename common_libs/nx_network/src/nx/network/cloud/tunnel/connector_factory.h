/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <list>
#include <map>

#include "abstract_cross_nat_connector.h"
#include "abstract_tunnel_connector.h"
#include "nx/network/cloud/address_resolver.h"
#include "nx/network/cloud/data/connect_data.h"


namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API ConnectorFactory
{
public:
    typedef std::list<std::unique_ptr<AbstractTunnelConnector>> CloudConnectors;
    typedef std::function<
        std::unique_ptr<AbstractCrossNatConnector>(
            const AddressEntry& /*targetAddress*/)> FactoryFunc;

    static CloudConnectors createCloudConnectors(
        const AddressEntry& targetAddress,
        const nx::String& connectSessionId,
        const hpm::api::ConnectResponse& response,
        std::unique_ptr<UDPSocket> udpSocket);

    static std::unique_ptr<AbstractCrossNatConnector>
        createCrossNatConnector(const AddressEntry& address);
    
    /** Replace factory. Debug only! */
    static FactoryFunc setFactoryFunc(FactoryFunc newFactoryFunc);
    /** Enable/disable cloud connectors. Debug only!
        @param cloudConnectTypeMask Bitset with values from \a CloudConnectType
        By default, everything is enabled
    */
    static void setEnabledCloudConnectMask(int cloudConnectTypeMask);
};

} // namespace cloud
} // namespace network
} // namespace nx
