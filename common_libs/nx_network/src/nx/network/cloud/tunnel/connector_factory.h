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
    
    /**
     * Replace factory with custom func. 
     * For debug purpose only!
     */
    static FactoryFunc setFactoryFunc(FactoryFunc newFactoryFunc);
    /**
     * Enable/disable cloud connectors. Debug only!
     * @param cloudConnectTypeMask Bitset with values from nx::network::cloud::CloudConnectType enum.
     * By default, everything is enabled.
     */
    static void setEnabledCloudConnectMask(int cloudConnectTypeMask);
    static int getEnabledCloudConnectMask();
};

} // namespace cloud
} // namespace network
} // namespace nx
