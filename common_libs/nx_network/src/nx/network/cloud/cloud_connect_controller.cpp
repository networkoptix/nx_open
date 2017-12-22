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
    hpm::api::MediatorConnector mediatorConnector;
    MediatorAddressPublisher addressPublisher;
    OutgoingTunnelPool outgoingTunnelPool;
    CloudConnectSettings settings;
    tcp::ReverseConnectionPool tcpReversePool;
    AddressResolver* m_addressResolver;

    CloudConnectControllerImpl(
        aio::AIOService* aioService,
        AddressResolver* addressResolver)
        :
        addressPublisher(mediatorConnector.systemConnection()),
        tcpReversePool(
            aioService,
            outgoingTunnelPool,
            mediatorConnector.clientConnection()),
        m_addressResolver(addressResolver)
    {
        mediatorConnector.setOnMediatorAvailabilityChanged(
            [this](bool isMediatorAvailable)
            {
                m_addressResolver->setCloudResolveEnabled(isMediatorAvailable);
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

        m_addressResolver->setCloudResolveEnabled(false);
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
    if (const auto value = arguments.get("enforce-mediator", "mediator"))
        mediatorConnector().mockupMediatorUrl(*value);

    if (arguments.get("cloud-connect-disable-udp"))
    {
        cloud::ConnectorFactory::setEnabledCloudConnectMask(
            cloud::ConnectorFactory::getEnabledCloudConnectMask() &
            ~((int)cloud::ConnectType::udpHp));
    }

    if (arguments.get("cloud-connect-enable-proxy-only"))
    {
        cloud::ConnectorFactory::setEnabledCloudConnectMask(
            (int)cloud::ConnectType::proxy);
    }
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

} // namespace cloud
} // namespace network
} // namespace nx
