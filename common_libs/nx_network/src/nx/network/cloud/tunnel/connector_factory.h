#pragma once

#include <chrono>
#include <list>
#include <map>

#include <nx/utils/basic_factory.h>

#include "abstract_cross_nat_connector.h"
#include "abstract_tunnel_connector.h"
#include "nx/network/cloud/address_resolver.h"
#include "nx/network/cloud/data/connect_data.h"

namespace nx {
namespace network {
namespace cloud {

struct TunnelConnectorContext
{
    std::unique_ptr<AbstractTunnelConnector> connector;
    std::chrono::milliseconds startDelay;
};

using CloudConnectors = std::list<TunnelConnectorContext>;

typedef CloudConnectors (ConnectorFactoryFunc)(
    const AddressEntry& /*targetAddress*/,
    const nx::String& /*connectSessionId*/,
    const hpm::api::ConnectResponse& /*response*/,
    std::unique_ptr<UDPSocket> /*udpSocket*/);

class NX_NETWORK_API ConnectorFactory:
    public nx::utils::BasicFactory<ConnectorFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<ConnectorFactoryFunc>;

public:
    ConnectorFactory();

    static ConnectorFactory& instance();

    /**
     * Enable/disable cloud connectors. Debug only!
     * @param cloudConnectTypeMask Bitset with values from nx::network::cloud::CloudConnectType enum.
     * By default, everything is enabled.
     */
    static void setEnabledCloudConnectMask(int cloudConnectTypeMask);
    static int getEnabledCloudConnectMask();

private:
    CloudConnectors defaultFunc(
        const AddressEntry& targetAddress,
        const nx::String& connectSessionId,
        const hpm::api::ConnectResponse& response,
        std::unique_ptr<UDPSocket> udpSocket);
};

//-------------------------------------------------------------------------------------------------

typedef std::unique_ptr<AbstractCrossNatConnector> (CrossNatConnectorFunc)(
    const AddressEntry&);

class NX_NETWORK_API CrossNatConnectorFactory:
    public nx::utils::BasicFactory<CrossNatConnectorFunc>
{
    using base_type = nx::utils::BasicFactory<CrossNatConnectorFunc>;

public:
    CrossNatConnectorFactory();

    static CrossNatConnectorFactory& instance();

private:
    std::unique_ptr<AbstractCrossNatConnector> defaultFunc(
        const AddressEntry& address);
};

} // namespace cloud
} // namespace network
} // namespace nx
