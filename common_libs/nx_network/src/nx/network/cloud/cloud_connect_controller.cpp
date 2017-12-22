#include "cloud_connect_controller.h"

#include <nx/network/address_resolver.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/barrier_handler.h>

#include "cloud_connect_settings.h"
#include "mediator_address_publisher.h"
#include "mediator_connector.h"
#include "tunnel/connector_factory.h"
#include "tunnel/outgoing_tunnel_pool.h"
#include "tunnel/tcp/reverse_connection_pool.h"

namespace nx {
namespace network {
namespace cloud {

struct CloudConnectControllerImpl
{
    aio::AIOService* aioService;
    AddressResolver* addressResolver;
    hpm::api::MediatorConnector mediatorConnector;
    MediatorAddressPublisher addressPublisher;
    OutgoingTunnelPool outgoingTunnelPool;
    CloudConnectSettings settings;
    tcp::ReverseConnectionPool tcpReversePool;

    CloudConnectControllerImpl(
        aio::AIOService* aioService,
        AddressResolver* addressResolver)
        :
        aioService(aioService),
        addressResolver(addressResolver),
        addressPublisher(mediatorConnector.systemConnection()),
        tcpReversePool(
            aioService,
            outgoingTunnelPool,
            mediatorConnector.clientConnection())
    {
        mediatorConnector.setOnMediatorAvailabilityChanged(
            [this](bool isMediatorAvailable)
            {
                this->addressResolver->setCloudResolveEnabled(isMediatorAvailable);
            });
    }

    ~CloudConnectControllerImpl()
    {
        nx::utils::promise<void> cloudServicesStoppedPromise;
        {
            utils::BarrierHandler barrier([&]() { cloudServicesStoppedPromise.set_value(); });
            addressPublisher.pleaseStop(barrier.fork());
            outgoingTunnelPool.pleaseStop(barrier.fork());
            tcpReversePool.pleaseStop(barrier.fork());
        }

        cloudServicesStoppedPromise.get_future().wait();

        addressResolver->setCloudResolveEnabled(false);
    }
};

//-------------------------------------------------------------------------------------------------

CloudConnectController::CloudConnectController(
    aio::AIOService* aioService,
    AddressResolver* addressResolver)
    :
    m_impl(std::make_unique<CloudConnectControllerImpl>(aioService, addressResolver))
{
}

CloudConnectController::~CloudConnectController()
{
}

void CloudConnectController::applyArguments(const utils::ArgumentParser& arguments)
{
    loadSettings(arguments);
    applySettings();
}

hpm::api::MediatorConnector& CloudConnectController::mediatorConnector()
{
    return m_impl->mediatorConnector;
}

MediatorAddressPublisher& CloudConnectController::addressPublisher()
{
    return m_impl->addressPublisher;
}

OutgoingTunnelPool& CloudConnectController::outgoingTunnelPool()
{
    return m_impl->outgoingTunnelPool;
}

CloudConnectSettings& CloudConnectController::settings()
{
    return m_impl->settings;
}

tcp::ReverseConnectionPool& CloudConnectController::tcpReversePool()
{
    return m_impl->tcpReversePool;
}

void CloudConnectController::reinitialize()
{
    auto aioService = m_impl->aioService;
    auto addressResolver = m_impl->addressResolver;
    const auto ownPeerId = outgoingTunnelPool().ownPeerId();

    m_impl.reset();

    m_impl = std::make_unique<CloudConnectControllerImpl>(aioService, addressResolver);
    applySettings();
    outgoingTunnelPool().setOwnPeerId(ownPeerId);
}

void CloudConnectController::printArgumentsHelp(std::ostream* outputStream)
{
    (*outputStream) <<
        "  --enforce-mediator={endpoint}    Enforces custom mediator address" << std::endl <<
        "  --cloud-connect-disable-udp      Disable UDP hole punching" << std::endl <<
        "  --cloud-connect-enable-proxy-only" << std::endl;
}

void CloudConnectController::loadSettings(const utils::ArgumentParser& arguments)
{
    if (const auto value = arguments.get("enforce-mediator", "mediator"))
        m_settings.forcedMediatorUrl = *value;

    // TODO: #ak Following parameters are redundant and contradicting.

    if (arguments.get("cloud-connect-disable-udp"))
        m_settings.isUdpHpDisabled = true;

    if (arguments.get("cloud-connect-enable-proxy-only"))
        m_settings.isOnlyCloudProxyEnabled = true;
}

void CloudConnectController::applySettings()
{
    if (!m_settings.forcedMediatorUrl.isEmpty())
        mediatorConnector().mockupMediatorUrl(m_settings.forcedMediatorUrl);

    if (m_settings.isUdpHpDisabled)
    {
        cloud::ConnectorFactory::setEnabledCloudConnectMask(
            cloud::ConnectorFactory::getEnabledCloudConnectMask() &
            ~((int)cloud::ConnectType::udpHp));
    }

    if (m_settings.isOnlyCloudProxyEnabled)
    {
        cloud::ConnectorFactory::setEnabledCloudConnectMask(
            (int)cloud::ConnectType::proxy);
    }
}

} // namespace cloud
} // namespace network
} // namespace nx
