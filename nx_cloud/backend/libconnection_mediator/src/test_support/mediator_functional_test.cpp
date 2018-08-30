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
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>

#include "http/get_listening_peer_list_handler.h"
#include "local_cloud_data_provider.h"
#include "mediator_service.h"

namespace nx {
namespace hpm {

static constexpr size_t kMaxBindRetryCount = 10;

MediatorFunctionalTest::MediatorFunctionalTest(int flags):
    utils::test::TestWithTemporaryDirectory("hpm", QString()),
    m_testFlags(flags),
    m_httpPort(0)
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
        AbstractCloudDataProviderFactory::setFactoryFunc(std::move(*m_factoryFuncToRestore));
        m_factoryFuncToRestore.reset();
    }
}

bool MediatorFunctionalTest::waitUntilStarted()
{
    if (!utils::test::ModuleLauncher<MediatorProcessPublic>::waitUntilStarted())
        return false;

    m_stunTcpEndpoint = moduleInstance()->impl()->stunTcpEndpoints().front();
    m_stunUdpEndpoint = moduleInstance()->impl()->stunUdpEndpoints().front();

    const auto& httpEndpoints = moduleInstance()->impl()->httpEndpoints();
    if (httpEndpoints.empty())
        return false;
    m_httpPort = httpEndpoints.front().port;

    if (m_testFlags & MediatorTestFlags::initializeConnectivity)
    {
        network::SocketGlobals::cloud().mediatorConnector().mockupMediatorUrl(
            nx::network::url::Builder()
                .setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(stunTcpEndpoint()),
            stunUdpEndpoint());
        network::SocketGlobals::cloud().mediatorConnector().enable(true);
    }

    return true;
}

network::SocketAddress MediatorFunctionalTest::stunUdpEndpoint() const
{
    return network::SocketAddress(
        network::HostAddress::localhost,
        moduleInstance()->impl()->stunUdpEndpoints().front().port);
}

network::SocketAddress MediatorFunctionalTest::stunTcpEndpoint() const
{
    return network::SocketAddress(
        network::HostAddress::localhost,
        moduleInstance()->impl()->stunTcpEndpoints().front().port);
}

network::SocketAddress MediatorFunctionalTest::httpEndpoint() const
{
    return network::SocketAddress(network::HostAddress::localhost, m_httpPort);
}

void MediatorFunctionalTest::setPreserveEndpointsDuringRestart(bool value)
{
    m_preserveEndpointsDuringRestart = value;
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
    m_factoryFuncToRestore =
        AbstractCloudDataProviderFactory::setFactoryFunc(
            [cloudDataProvider](
                const std::optional<nx::utils::Url>& /*cdbUrl*/,
                const std::string& /*user*/,
                const std::string& /*password*/,
                std::chrono::milliseconds /*updateInterval*/,
                std::chrono::milliseconds /*startTimeout*/)
            {
                return std::make_unique<CloudDataProviderStub>(cloudDataProvider);
            });
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
        NX_LOGX(lm("Failed to bind server: %1, endpoint=%2")
            .arg(server->fullName()).arg(server->endpoint()), cl_logERROR);
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

std::tuple<nx::network::http::StatusCode::Value, api::ListeningPeers>
    MediatorFunctionalTest::getListeningPeers() const
{
    api::Client mediatorClient(
        nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(httpEndpoint()).setPath(api::kMediatorApiPrefix).toUrl());
    return mediatorClient.getListeningPeers();
}

void MediatorFunctionalTest::beforeModuleCreation()
{
    if (!m_preserveEndpointsDuringRestart)
        return;

    for (auto it = args().begin(); it != args().end(); )
    {
        if (strcmp((*it), "-stun/addrToListenList") == 0 ||
            strcmp((*it), "-http/addrToListenList") == 0)
        {
            free(*it);
            it = args().erase(it);
            free(*it);
            it = args().erase(it); //< Value.
        }
        else
        {
            ++it;
        }
    }

    network::SocketAddress httpEndpoint = network::SocketAddress::anyPrivateAddressV4;
    if (m_httpPort != 0)
        httpEndpoint.port = m_httpPort;

    addArg("-stun/addrToListenList", m_stunTcpEndpoint.toStdString().c_str());
    addArg("-stun/udpAddrToListenList", m_stunUdpEndpoint.toStdString().c_str());
    addArg("-http/addrToListenList", httpEndpoint.toStdString().c_str());
}

void MediatorFunctionalTest::afterModuleDestruction()
{
    //clearArgs();
}

} // namespace hpm
} // namespace nx
