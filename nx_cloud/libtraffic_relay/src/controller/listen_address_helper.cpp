#include <nx/network/public_ip_discovery.h>
#include <nx/network/nettools.h>

#include "listen_address_helper.h"

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

AbstractListenAddressHelper::AbstractListenAddressHelper(
    AbstractListenAddressHelperHandler* handler)
    :
    m_handler(handler)
{
}

void AbstractListenAddressHelper::findBestAddress(
    const std::vector<SocketAddress>& httpEndpoints) const
{
    NX_ASSERT(!httpEndpoints.empty());
    if (httpEndpoints.empty())
    {
        NX_WARNING(this, "Endpoints list is empty");
        return;
    }

    if (!(bool) publicAddress())
    {
        doReport(
            httpEndpoints[0],
            lm("Failed to discover public relay host address. Reporting first available (%1)"),
            utils::log::Level::warning);

        return;
    }

    for (const auto& listenAddress: httpEndpoints)
    {
        if (endpointReportedAsPublic(listenAddress, *publicAddress()))
            return;
    }

    doReport(
        httpEndpoints[0],
        lm("Public address discovered but it wasn't be found amongst listen addresses. \
                Reporting first available (%1)"),
        utils::log::Level::warning);
}

void AbstractListenAddressHelper::doReport(
    const SocketAddress& address,
    const lm& message,
    utils::log::Level logLevel) const
{
    SocketAddress result = address;
    if (address.address == HostAddress::anyHost)
    {
        auto firstAvailableAddress = allLocalIpV4Addresses()[0];
        result = SocketAddress(firstAvailableAddress.toString(), address.port);
    }

    auto endpointString = result.toStdString();
    NX_UTILS_LOG(logLevel, this, message.arg(endpointString));

    NX_ASSERT(m_handler);
    m_handler->onBestEndpointDiscovered(endpointString);
}

bool AbstractListenAddressHelper::endpointReportedAsPublic(
    const SocketAddress& listenAddress,
    const HostAddress& publicAddress) const
{
    if (listenAddress.address == publicAddress)
    {
        doReport(
            listenAddress,
            lm("Found public address (%1) amongst listen addresses."),
            utils::log::Level::info);

        return true;
    }

    if (listenAddress.address != HostAddress::anyHost)
        return false;

    for (const auto& availableQHostAddress : allLocalIpV4Addresses())
    {
        HostAddress availableAddress(availableQHostAddress.toString());
        if (availableAddress == publicAddress)
        {
            doReport(
                listenAddress,
                lm("Found public address (%1) amongst listen addresses."),
                utils::log::Level::info);

            return true;
        }
    }

    return false;
}

ListenAddressHelper::ListenAddressHelper(AbstractListenAddressHelperHandler* handler):
    AbstractListenAddressHelper(handler)
{
    network::PublicIPDiscovery ipDiscovery;

    ipDiscovery.update();
    ipDiscovery.waitForFinished();
    auto addr =  ipDiscovery.publicIP();

    if (!addr.isNull())
        m_publicAddress = HostAddress(addr.toString());
}

boost::optional<HostAddress> ListenAddressHelper::publicAddress() const
{
    return m_publicAddress;
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx