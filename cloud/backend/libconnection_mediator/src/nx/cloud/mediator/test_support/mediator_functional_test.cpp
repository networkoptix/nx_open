#include "mediator_functional_test.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
#include <tuple>

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket.h>
#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>

#include "local_cloud_data_provider.h"
#include "../http/get_listening_peer_list_handler.h"
#include "../mediator_service.h"

namespace nx {
namespace hpm {

static constexpr size_t kMaxBindRetryCount = 10;

MediatorFunctionalTest::MediatorFunctionalTest(int flags, const QString& testDir):
    utils::test::TestWithTemporaryDirectory("hpm", testDir),
    m_testFlags(flags)
{
    if (m_testFlags & initializeSocketGlobals)
        nx::network::SocketGlobals::cloud().reinitialize();

    addArg("/path/to/bin");
    addArg("-e");
    addArg("-stun/addrToListenList", "127.0.0.1:0");
    addArg("-stun/udpAddrToListenList", "127.0.0.1:0");
    addArg("-http/addrToListenList", "127.0.0.1:0");
    addArg("-log/logLevel", "DEBUG2");
    addArg("-general/dataDir", testDataDir().toLatin1().constData());

    if (m_testFlags & MediatorTestFlags::useTestCloudDataProvider)
        registerCloudDataProvider(&m_cloudDataProvider);
}

MediatorFunctionalTest::~MediatorFunctionalTest()
{
    stop();

    if (m_factoryFuncToRestore)
    {
        AbstractCloudDataProviderFactory::setFactoryFunc(
            std::move(*m_factoryFuncToRestore));
        m_factoryFuncToRestore.reset();
    }
}

void MediatorFunctionalTest::setUseProxy(bool value)
{
    m_useProxy = value;
}

bool MediatorFunctionalTest::waitUntilStarted()
{
    ++m_startCount;

    NX_ASSERT(m_startCount == 1 || m_useProxy);

    if (!utils::test::ModuleLauncher<MediatorProcessPublic>::waitUntilStarted())
        return false;

    m_stunTcpEndpoint = moduleInstance()->impl()->stunTcpEndpoints().front();
    // Proxy is needed to be able to restart mediator while preserving same ports.
    if (m_useProxy)
    {
        if (!startProxy())
            return false;
        m_httpEndpoint = m_httpProxy.endpoint;
        m_stunTcpEndpoint = m_stunProxy.endpoint;
    }
    else
    {
        m_stunTcpEndpoint = moduleInstance()->impl()->stunTcpEndpoints().front();
        m_httpEndpoint = moduleInstance()->impl()->httpEndpoints().front();
    }

    if (!moduleInstance()->impl()->stunUdpEndpoints().empty())
        m_stunUdpEndpoint = moduleInstance()->impl()->stunUdpEndpoints().front();

    if (m_testFlags & MediatorTestFlags::initializeConnectivity)
    {
        network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            nx::network::url::Builder()
                .setScheme(nx::network::stun::kUrlSchemeName)
                .setEndpoint(stunTcpEndpoint()),
            stunUdpEndpoint()});
    }

    return true;
}

network::SocketAddress MediatorFunctionalTest::stunUdpEndpoint() const
{
    if (moduleInstance()->impl()->stunUdpEndpoints().empty())
        return network::SocketAddress();

    return network::SocketAddress(
        network::HostAddress::localhost,
        moduleInstance()->impl()->stunUdpEndpoints().front().port);
}

network::SocketAddress MediatorFunctionalTest::stunTcpEndpoint() const
{
    return m_stunTcpEndpoint;
}

network::SocketAddress MediatorFunctionalTest::httpEndpoint() const
{
    return m_httpEndpoint;
}

nx::utils::Url MediatorFunctionalTest::httpUrl() const
{
    return nx::network::url::Builder()
        .setScheme(network::http::kUrlSchemeName)
        .setEndpoint(m_httpEndpoint)
        .setPath(hpm::api::kMediatorApiPrefix).toUrl();
}

std::unique_ptr<nx::hpm::api::MediatorClientTcpConnection>
    MediatorFunctionalTest::clientConnection()
{
    return network::SocketGlobals::cloud().mediatorConnector().clientConnection();
}

std::unique_ptr<nx::hpm::api::MediatorServerTcpConnection>
    MediatorFunctionalTest::systemConnection()
{
    return network::SocketGlobals::cloud().mediatorConnector().systemConnection();
}

void MediatorFunctionalTest::registerCloudDataProvider(
    AbstractCloudDataProvider* cloudDataProvider)
{
    auto oldFactoryFunc =
        AbstractCloudDataProviderFactory::setFactoryFunc(
            [cloudDataProvider](auto&&...)
            {
                return std::make_unique<CloudDataProviderStub>(cloudDataProvider);
            });

    if (!m_factoryFuncToRestore)
        m_factoryFuncToRestore = std::move(oldFactoryFunc);
}

AbstractCloudDataProvider::System MediatorFunctionalTest::addRandomSystem()
{
    AbstractCloudDataProvider::System system(
        QnUuid::createUuid().toSimpleString().toUtf8(),
        nx::utils::generateRandomName(16),
        true);
    m_cloudDataProvider.addSystem(
        system.id,
        system);
    return system;
}

std::unique_ptr<MediaServerEmulator> MediatorFunctionalTest::addServer(
    const AbstractCloudDataProvider::System& system,
    nx::String name,
    ServerTweak::Value tweak)
{
    auto server = std::make_unique<MediaServerEmulator>(
        stunUdpEndpoint(),
        stunTcpEndpoint(),
        system,
        std::move(name));

    if (!server->start(!(tweak & ServerTweak::noListenToConnect)))
    {
        NX_ERROR(this, lm("Failed to start server: %1").arg(server->fullName()));
        return nullptr;
    }

    if (!(tweak & ServerTweak::noBindEndpoint)
        && server->bind() != nx::hpm::api::ResultCode::ok)
    {
        NX_ERROR(this, lm("Failed to bind server: %1, endpoint=%2")
            .arg(server->fullName()).arg(server->endpoint()));
        return nullptr;
    }

    return server;
}

std::unique_ptr<MediaServerEmulator> MediatorFunctionalTest::addRandomServer(
    const AbstractCloudDataProvider::System& system,
    boost::optional<QnUuid> serverId,
    ServerTweak::Value tweak)
{
    if (!serverId)
        serverId = QnUuid::createUuid();

    return addServer(
        system,
        serverId.get().toSimpleString().toUtf8(),
        tweak);
}

std::vector<std::unique_ptr<MediaServerEmulator>>
    MediatorFunctionalTest::addRandomServers(
        const AbstractCloudDataProvider::System& system,
        size_t count, ServerTweak::Value tweak)
{
    std::vector<std::unique_ptr<MediaServerEmulator>> systemServers;
    for (size_t i = 0; i < count; ++i)
    {
        auto server = addRandomServer(system, boost::none, tweak);
        if (!server)
            return {};

        systemServers.push_back(std::move(server));
    }

    return systemServers;
}

std::tuple<api::ResultCode, api::ListeningPeers>
    MediatorFunctionalTest::getListeningPeers() const
{
    api::Client mediatorClient(
        nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(httpEndpoint()).setPath(api::kMediatorApiPrefix).toUrl());
    return mediatorClient.getListeningPeers();
}

void MediatorFunctionalTest::beforeModuleCreation()
{
    if (m_stunUdpEndpoint)
    {
        removeArgByName("stun/udpAddrToListenList");
        addArg("-stun/udpAddrToListenList", m_stunUdpEndpoint->toStdString().c_str());
    }
}

void MediatorFunctionalTest::afterModuleDestruction()
{
    //clearArgs();
}

bool MediatorFunctionalTest::startProxy()
{
    if (!m_tcpPortsAllocated)
    {
        if (!allocateTcpPorts())
            return false;
        m_tcpPortsAllocated = true;
    }

    m_httpProxy.server->setProxyDestination(
        moduleInstance()->impl()->httpEndpoints().front());
    m_stunProxy.server->setProxyDestination(
        moduleInstance()->impl()->stunTcpEndpoints().front());

    return true;
}

bool MediatorFunctionalTest::allocateTcpPorts()
{
    auto proxyToInitialize = {&m_httpProxy, &m_stunProxy};

    for (auto proxy: proxyToInitialize)
    {
        auto serverSocket = std::make_unique<nx::network::TCPServerSocket>(AF_INET);
        if (!serverSocket->setNonBlockingMode(true))
            return false;
        if (!serverSocket->bind(nx::network::SocketAddress::anyPrivateAddressV4))
            return false;
        if (!serverSocket->listen())
            return false;

        proxy->server = std::make_unique<nx::network::StreamProxy>();
        proxy->endpoint = serverSocket->getLocalAddress();
        proxy->server->startProxy(
            std::make_unique<nx::network::StreamServerSocketToAcceptorWrapper>(
                std::move(serverSocket)),
            nx::network::SocketAddress());
    }

    return true;
}

} // namespace hpm
} // namespace nx
