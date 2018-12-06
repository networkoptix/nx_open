#include "cloud_connect_controller.h"

#include <nx/network/address_resolver.h>
#include <nx/network/app_info.h>
#include <nx/network/url/url_parse_helper.h>
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

    CloudConnectControllerImpl(
        const QString& customCloudHost,
        aio::AIOService* aioService,
        AddressResolver* addressResolver)
        :
        cloudHost(!customCloudHost.isEmpty() ? customCloudHost : AppInfo::defaultCloudHostName()),
        aioService(aioService),
        addressResolver(addressResolver),
        mediatorConnector(cloudHost.toStdString()),
        addressPublisher(
            mediatorConnector.systemConnection(),
            &mediatorConnector)
    {
    }

    ~CloudConnectControllerImpl()
    {
        nx::utils::promise<void> cloudServicesStoppedPromise;
        {
            utils::BarrierHandler barrier([&]() { cloudServicesStoppedPromise.set_value(); });
            addressPublisher.pleaseStop(barrier.fork());
            outgoingTunnelPool.pleaseStop(barrier.fork());
        }

        cloudServicesStoppedPromise.get_future().wait();
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
        "  --cloud-connect-disable-direct-tcp" << std::endl <<
        "  --cloud-connect-enable-proxy-only" << std::endl <<
        "  --cloud-connect-disable-proxy" << std::endl;
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
        m_impl->settings.isUdpHpEnabled = false;

    if (arguments.get("cloud-connect-disable-direct-tcp"))
        m_impl->settings.isDirectTcpConnectEnabled = false;

    if (!static_cast<bool>(arguments.get("cloud-connect-enable-proxy-only")) &&
        arguments.get("cloud-connect-disable-proxy"))
    {
        m_impl->settings.isCloudProxyEnabled = false;
    }

    if (arguments.get("cloud-connect-enable-proxy-only"))
    {
        m_impl->settings.isUdpHpEnabled = false;
        m_impl->settings.isDirectTcpConnectEnabled = false;
        m_impl->settings.isCloudProxyEnabled = true;
    }
}

void CloudConnectController::applySettings()
{
    if (!m_impl->settings.forcedMediatorUrl.empty())
    {
        mediatorConnector().mockupMediatorAddress({
            utils::Url(m_impl->settings.forcedMediatorUrl),
            url::getEndpoint(utils::Url(m_impl->settings.forcedMediatorUrl))});
    }

    int enabledConnectTypes = static_cast<int>(cloud::ConnectType::all);

    if (!m_impl->settings.isCloudProxyEnabled)
        enabledConnectTypes &= ~(int)cloud::ConnectType::proxy;

    if (!m_impl->settings.isUdpHpEnabled)
        enabledConnectTypes &= ~(int)cloud::ConnectType::udpHp;

    if (!m_impl->settings.isDirectTcpConnectEnabled)
        enabledConnectTypes &= ~(int)cloud::ConnectType::forwardedTcpPort;

    cloud::ConnectorFactory::setEnabledCloudConnectMask(
        cloud::ConnectorFactory::getEnabledCloudConnectMask() &
        enabledConnectTypes);
}

} // namespace cloud
} // namespace network
} // namespace nx
