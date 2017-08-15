#include "view.h"

#include <stdexcept>

#include <nx/network/public_ip_discovery.h>
#include <nx/network/nettools.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "http_handlers.h"
#include "../controller/connect_session_manager.h"
#include "../controller/controller.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {

View::View(
    const conf::Settings& settings,
    const Model& /*model*/,
    Controller* controller)
    :
    m_settings(settings),
    m_controller(controller),
    m_authenticationManager(m_authRestrictionList)
{
    registerApiHandlers();
    startAcceptor();
}

View::~View()
{
    m_multiAddressHttpServer->forEachListener(
        [](nx_http::HttpStreamSocketServer* listener)
        {
            listener->pleaseStopSync();
        });
}

void View::start()
{
    if (!m_multiAddressHttpServer->listen(m_settings.http().tcpBacklogSize))
    {
        throw std::runtime_error(
            lm("Cannot start listening: %1")
            .arg(SystemError::getLastOSErrorText()).toStdString());
    }
}

std::vector<SocketAddress> View::httpEndpoints() const
{
    return m_multiAddressHttpServer->endpoints();
}

void View::registerApiHandlers()
{
    registerApiHandler<view::BeginListeningHandler>(nx_http::Method::options);
    registerApiHandler<view::CreateClientSessionHandler>(nx_http::Method::post);
    registerApiHandler<view::ConnectToPeerHandler>(nx_http::Method::options);
}

template<typename Handler>
void View::registerApiHandler(const nx_http::StringType& method)
{
    m_httpMessageDispatcher.registerRequestProcessor<Handler>(
        Handler::kPath,
        [this]() -> std::unique_ptr<Handler>
        {
            return std::make_unique<Handler>(&m_controller->connectSessionManager());
        },
        method);
}

void View::startAcceptor()
{
    const auto& httpEndpoints = m_settings.http().endpoints;
    if (httpEndpoints.empty())
    {
        NX_LOGX("No HTTP address to listen", cl_logALWAYS);
        throw std::runtime_error("No HTTP address to listen");
    }

    m_multiAddressHttpServer =
        std::make_unique<nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer>>(
            &m_authenticationManager,
            &m_httpMessageDispatcher,
            false,
            nx::network::NatTraversalSupport::disabled);

    if (!m_multiAddressHttpServer->bind(httpEndpoints))
    {
        throw std::runtime_error(
            lm("Cannot bind to address(es) %1. %2")
                .container(httpEndpoints)
                .arg(SystemError::getLastOSErrorText()).toStdString());
    }

    reportPublicListenAddress();
}

void View::reportPublicListenAddress()
{
    NX_ASSERT(!m_multiAddressHttpServer->endpoints().empty());

    network::PublicIPDiscovery ipDiscovery;

    ipDiscovery.update();
    ipDiscovery.waitForFinished();
    QHostAddress publicAddress = ipDiscovery.publicIP();

    if (publicAddress.isNull())
    {
        doReport(
            m_multiAddressHttpServer->endpoints()[0],
            lm("Failed to discover public relay host address. Reporting first available (%1)"),
            utils::log::Level::warning);

        return;
    }

    for (const auto& listenAddress: m_multiAddressHttpServer->endpoints())
    {
        if (endpointReportedAsPublic(listenAddress, publicAddress))
            return;
    }

    doReport(
        m_multiAddressHttpServer->endpoints()[0],
        lm("Public address discovered but it wasn't be found amongst listen addresses. \
                Reporting first available (%1)"),
        utils::log::Level::warning);
}

void View::doReport(const SocketAddress& address, const lm& message, utils::log::Level logLevel)
{
    SocketAddress result = address;
    if (address.address == HostAddress::anyHost)
    {
        auto firstAvailableAddress = allLocalAddresses()[0];
        result = SocketAddress(firstAvailableAddress.toString(), address.port);
    }

    auto addressString = result.toStdString();
    NX_UTILS_LOG(logLevel, this, message.arg(addressString));

    try
    {
        auto& reportAddressHandler =
            dynamic_cast<controller::AbstractReportPublicAddressHandler&>(
                m_controller->connectSessionManager());

        reportAddressHandler.onPublicAddressDiscovered(addressString);
    }
    catch (const std::exception&)
    {
        NX_CRITICAL("Cast to the AbstractReportPublicAddressHandler failed.");
    }
}

bool View::endpointReportedAsPublic(
    const SocketAddress& listenAddress,
    const QHostAddress& publicAddress)
{
    if (listenAddress.toString() == publicAddress.toString())
    {
        doReport(
            listenAddress,
            lm("Found public address (%1) amongst listen addresses."),
            utils::log::Level::info);

        return true;
    }

    if (listenAddress.address == HostAddress::anyHost)
    {
        for (const auto& availableAddress : allLocalAddresses())
        {
            if (availableAddress == publicAddress)
            {
                doReport(
                    listenAddress,
                    lm("Found public address (%1) amongst listen addresses."),
                    utils::log::Level::info);

                return true;
            }
        }
    }

    return false;
}

} // namespace relay
} // namespace cloud
} // namespace nx
