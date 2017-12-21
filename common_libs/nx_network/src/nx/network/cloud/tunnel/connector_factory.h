#pragma once

#include <chrono>
#include <list>
#include <map>

#include <nx/network/system_socket.h>
#include <nx/utils/basic_factory.h>

#include "abstract_cross_nat_connector.h"
#include "abstract_tunnel_connector.h"
#include "../address_resolver.h"
#include "../cloud_connect_type.h"
#include "../data/connect_data.h"

namespace nx {
namespace network {
namespace cloud {

struct TunnelConnectorContext
{
    std::unique_ptr<AbstractTunnelConnector> connector;
    std::chrono::milliseconds startDelay = std::chrono::milliseconds::zero();
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
     * @param cloudConnectTypeMask Bitset with values from nx::network::cloud::ConnectType enum.
     * By default, everything is enabled.
     */
    static void setEnabledCloudConnectMask(int cloudConnectTypeMask);
    static int getEnabledCloudConnectMask();

private:
    CloudConnectors defaultFactoryFunction(
        const AddressEntry& targetAddress,
        const nx::String& connectSessionId,
        const hpm::api::ConnectResponse& response,
        std::unique_ptr<UDPSocket> udpSocket);
};

//-------------------------------------------------------------------------------------------------

using CrossNatConnectorFactoryFunction =
    std::unique_ptr<AbstractCrossNatConnector>(const AddressEntry&);

class NX_NETWORK_API CrossNatConnectorFactory:
    public nx::utils::BasicFactory<CrossNatConnectorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<CrossNatConnectorFactoryFunction>;

public:
    CrossNatConnectorFactory();

    static CrossNatConnectorFactory& instance();

private:
    std::unique_ptr<AbstractCrossNatConnector> defaultFactoryFunction(
        const AddressEntry& address);
};

} // namespace cloud
} // namespace network
} // namespace nx
