// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connector_factory.h"

#include <nx/network/socket_global.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/url_query.h>

#include "cross_nat_connector.h"
#include "relay/relay_connector.h"
#include "tcp/direct_endpoint_connector.h"
#include "udp/connector.h"

namespace nx::network::cloud {

static int s_cloudConnectTypeMask = (int)ConnectType::all;

using namespace std::placeholders;

ConnectorFactory::ConnectorFactory():
    base_type(std::bind(&ConnectorFactory::defaultFactoryFunction, this, _1, _2, _3, _4))
{
}

ConnectorFactory& ConnectorFactory::instance()
{
    static ConnectorFactory instance;
    return instance;
}

CloudConnectors ConnectorFactory::defaultFactoryFunction(
    const AddressEntry& targetAddress,
    const std::string& connectSessionId,
    const hpm::api::ConnectResponse& response,
    std::unique_ptr<AbstractDatagramSocket> udpSocket)
{
    CloudConnectors connectors;

    if ((s_cloudConnectTypeMask & (int)ConnectType::udpHp) > 0 &&
        udpSocket && !response.udpEndpointList.empty())
    {
        TunnelConnectorContext context;
        context.connector = std::make_unique<udp::TunnelConnector>(
            targetAddress,
            connectSessionId,
            std::move(udpSocket));
        context.startDelay = response.params.udpHolePunchingStartDelay;
        connectors.emplace_back(std::move(context));
    }

    if (((s_cloudConnectTypeMask & (int)ConnectType::forwardedTcpPort) > 0) &&
        !response.forwardedTcpEndpointList.empty())
    {
        TunnelConnectorContext context;
        context.connector = std::make_unique<tcp::DirectEndpointConnector>(
            targetAddress, connectSessionId);
        context.startDelay = response.params.directTcpConnectStartDelay;
        connectors.emplace_back(std::move(context));
    }

    if (((s_cloudConnectTypeMask & (int)ConnectType::proxy) > 0) &&
        (!response.trafficRelayUrls.empty() || response.trafficRelayUrl))
    {
        auto relayUrls = response.trafficRelayUrls;
        // Old mediator compatibility check.
        if (response.trafficRelayUrls.empty() && response.trafficRelayUrl)
            relayUrls.push_back(*response.trafficRelayUrl);

        for (const auto& urlStr: relayUrls)
        {
            nx::utils::Url url(urlStr);
            TunnelConnectorContext context;
            context.startDelay = response.params.trafficRelayingStartDelay;

            nx::utils::UrlQuery urlQuery(url.query());
            if (urlQuery.contains(nx::hpm::api::kDelayParamName))
            {
                context.startDelay += nx::utils::parseTimerDuration(
                    urlQuery.queryItemValue<std::string>(nx::hpm::api::kDelayParamName));
                urlQuery.removeQueryItem(nx::hpm::api::kDelayParamName);
                url.setQuery(urlQuery);
            }

            context.connector = std::make_unique<relay::Connector>(
                url,
                targetAddress,
                connectSessionId);

            connectors.emplace_back(std::move(context));
        }
    }

    return connectors;
}

int ConnectorFactory::setEnabledCloudConnectMask(int cloudConnectTypeMask)
{
    const auto bak = s_cloudConnectTypeMask;
    s_cloudConnectTypeMask = cloudConnectTypeMask;
    return bak;
}

int ConnectorFactory::getEnabledCloudConnectMask()
{
    return s_cloudConnectTypeMask;
}

//-------------------------------------------------------------------------------------------------

CrossNatConnectorFactory::CrossNatConnectorFactory():
    base_type([this](auto&&... args)
        { return defaultFactoryFunction(std::forward<decltype(args)>(args)...); })
{
}

CrossNatConnectorFactory& CrossNatConnectorFactory::instance()
{
    static CrossNatConnectorFactory instance;
    return instance;
}

std::unique_ptr<AbstractCrossNatConnector> CrossNatConnectorFactory::defaultFactoryFunction(
    const std::string& connectSessionId,
    const AddressEntry& address)
{
    return std::make_unique<CrossNatConnector>(
        &SocketGlobals::cloud(),
        connectSessionId,
        address);
}

} // namespace nx::network::cloud
