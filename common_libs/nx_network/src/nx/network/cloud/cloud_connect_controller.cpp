#include "cloud_connect_controller.h"

#include <nx/network/address_resolver.h>
#include <nx/network/app_info.h>
#include <nx/kit/ini_config.h>
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

struct Ini:
    nx::kit::IniConfig
{
    Ini():
        IniConfig("nx_cloud_connect.ini")
    {
        reload();
    }

    NX_INI_FLAG(0, useHttpConnectToListenOnRelay,
        "Use HTTP CONNECT method to open server-side connection to the relay service");
};

//-------------------------------------------------------------------------------------------------

struct CloudConnectControllerImpl
{
    Ini ini;
    QString cloudHost;
    aio::AIOService* aioService;
    AddressResolver* addressResolver;
    hpm::api::MediatorConnector mediatorConnector;
    MediatorAddressPublisher addressPublisher;
    OutgoingTunnelPool outgoingTunnelPool;
    CloudConnectSettings settings;
    tcp::ReverseConnectionPool tcpReversePool;

    CloudConnectControllerImpl(
        const QString& customCloudHost,
        aio::AIOService* aioService,
        AddressResolver* addressResolver)
        :
        cloudHost(!customCloudHost.isEmpty() ? customCloudHost : AppInfo::defaultCloudHostName()),
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
    const QString& customCloudHost,
    aio::AIOService* aioService,
    AddressResolver* addressResolver)
    :
    m_impl(std::make_unique<CloudConnectControllerImpl>(
        customCloudHost, aioService, addressResolver))
{
    readSettingsFromIni();
}

CloudConnectController::~CloudConnectController()
{
}

void CloudConnectController::applyArguments(const utils::ArgumentParser& arguments)
{
    loadSettings(arguments);
    applySettings();
}

const QString& CloudConnectController::cloudHost() const
{
    return m_impl->cloudHost;
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
    auto cloudHost = m_impl->cloudHost;
    auto aioService = m_impl->aioService;
    auto addressResolver = m_impl->addressResolver;
    auto settings = m_impl->settings;
    const auto ownPeerId = outgoingTunnelPool().ownPeerId();

    m_impl.reset();

    m_impl = std::make_unique<CloudConnectControllerImpl>(
        cloudHost, aioService, addressResolver);
    m_impl->settings = settings;
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

void CloudConnectController::readSettingsFromIni()
{
    m_impl->ini.reload();

    m_impl->settings.useHttpConnectToListenOnRelay =
        m_impl->ini.useHttpConnectToListenOnRelay;
}

void CloudConnectController::loadSettings(const utils::ArgumentParser& arguments)
{
    if (const auto value = arguments.get("enforce-mediator", "mediator"))
        m_impl->settings.forcedMediatorUrl = value->toStdString();

    if (const auto value = arguments.get<int>("use-http-connect-to-listen-on-relay"))
        m_impl->settings.useHttpConnectToListenOnRelay = *value > 0;

    // TODO: #ak Following parameters are redundant and contradicting.

    if (arguments.get("cloud-connect-disable-udp"))
        m_impl->settings.isUdpHpDisabled = true;

    if (arguments.get("cloud-connect-enable-proxy-only"))
        m_impl->settings.isOnlyCloudProxyEnabled = true;
}

void CloudConnectController::applySettings()
{
    if (!m_impl->settings.forcedMediatorUrl.empty())
    {
        mediatorConnector().mockupMediatorUrl(
            QString::fromStdString(m_impl->settings.forcedMediatorUrl));
    }

    if (m_impl->settings.isUdpHpDisabled)
    {
        cloud::ConnectorFactory::setEnabledCloudConnectMask(
            cloud::ConnectorFactory::getEnabledCloudConnectMask() &
            ~((int)cloud::ConnectType::udpHp));
    }

    if (m_impl->settings.isOnlyCloudProxyEnabled)
    {
        cloud::ConnectorFactory::setEnabledCloudConnectMask(
            (int)cloud::ConnectType::proxy);
    }
}

} // namespace cloud
} // namespace network
} // namespace nx
