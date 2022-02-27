// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <list>
#include <map>

#include <nx/network/abstract_socket.h>
#include <nx/network/address_resolver.h>
#include <nx/utils/basic_factory.h>

#include "abstract_cross_nat_connector.h"
#include "abstract_tunnel_connector.h"
#include "../cloud_connect_type.h"
#include "../data/connect_data.h"

namespace nx::network::cloud {

struct TunnelConnectorContext
{
    std::unique_ptr<AbstractTunnelConnector> connector;
    std::chrono::milliseconds startDelay = std::chrono::milliseconds::zero();
};

using CloudConnectors = std::list<TunnelConnectorContext>;

typedef CloudConnectors (ConnectorFactoryFunc)(
    const AddressEntry& /*targetAddress*/,
    const std::string& /*connectSessionId*/,
    const hpm::api::ConnectResponse& /*response*/,
    std::unique_ptr<AbstractDatagramSocket> /*udpSocket*/);

class NX_NETWORK_API ConnectorFactory:
    public nx::utils::BasicFactory<ConnectorFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<ConnectorFactoryFunc>;

public:
    ConnectorFactory();

    static ConnectorFactory& instance();

    /**
     * Enable/disable cloud connectors. Debug only!
     * By default, everything is enabled.
     * @param cloudConnectTypeMask Bitset with values from nx::network::cloud::ConnectType enum.
     * @return The initial value.
     */
    static int setEnabledCloudConnectMask(int cloudConnectTypeMask);
    static int getEnabledCloudConnectMask();

private:
    CloudConnectors defaultFactoryFunction(
        const AddressEntry& targetAddress,
        const std::string& connectSessionId,
        const hpm::api::ConnectResponse& response,
        std::unique_ptr<AbstractDatagramSocket> udpSocket);
};

//-------------------------------------------------------------------------------------------------

using CrossNatConnectorFactoryFunction =
    std::unique_ptr<AbstractCrossNatConnector>(
        const std::string& connectSessionId,
        const AddressEntry&);

class NX_NETWORK_API CrossNatConnectorFactory:
    public nx::utils::BasicFactory<CrossNatConnectorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<CrossNatConnectorFactoryFunction>;

public:
    CrossNatConnectorFactory();

    static CrossNatConnectorFactory& instance();

private:
    std::unique_ptr<AbstractCrossNatConnector> defaultFactoryFunction(
        const std::string& connectSessionId,
        const AddressEntry& address);
};

} // namespace nx::network::cloud
