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

static SocketAddress findFreeTcpAndUdpLocalAddress()
{
    for (size_t attempt = 0; attempt < kMaxBindRetryCount; ++attempt)
    {
        const SocketAddress address(
            HostAddress::localhost,
            nx::utils::random::number<uint16_t>(5000, 50000));

        network::TCPServerSocket tcpSocket(AF_INET);
        if (!tcpSocket.bind(address))
            continue;

        network::UDPSocket udpSocket(AF_INET);
        if (!udpSocket.bind(address))
            continue;

        return address;
    }

    return SocketAddress::anyPrivateAddress;
}

MediatorFunctionalTest::MediatorFunctionalTest(int flags):
    m_testFlags(flags),
    m_stunPort(0),
    m_httpPort(0)
{
    if (m_testFlags & initializeSocketGlobals)
        nx::network::SocketGlobals::cloud().reinitialize();

    m_tmpDir = QDir::homePath() + "/hpm_ut.data";
    QDir(m_tmpDir).removeRecursively();
    QDir().mkpath(m_tmpDir);

    m_stunAddress = findFreeTcpAndUdpLocalAddress();
    NX_LOGX(lm("STUN TCP & UDP endpoint: %1").arg(m_stunAddress), cl_logINFO);

    addArg("/path/to/bin");
    addArg("-e");
    addArg("-stun/addrToListenList", m_stunAddress.toStdString().c_str());
    addArg("-http/addrToListenList", SocketAddress::anyPrivateAddress.toStdString().c_str());
    addArg("-log/logLevel", "DEBUG2");
    addArg("-general/dataDir", m_tmpDir.toLatin1().constData());

    if (m_testFlags & MediatorTestFlags::useTestCloudDataProvider)
        registerCloudDataProvider(&m_cloudDataProvider);
}

MediatorFunctionalTest::~MediatorFunctionalTest()
{
    stop();

    QDir(m_tmpDir).removeRecursively();

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

    const auto& stunEndpoints = moduleInstance()->impl()->stunEndpoints();
    if (stunEndpoints.empty())
        return false;
    m_stunPort = stunEndpoints.front().port;

    const auto& httpEndpoints = moduleInstance()->impl()->httpEndpoints();
    if (httpEndpoints.empty())
        return false;
    m_httpPort = httpEndpoints.front().port;

    if (m_testFlags & MediatorTestFlags::initializeConnectivity)
    {
        network::SocketGlobals::cloud().mediatorConnector().mockupMediatorUrl(
            nx::network::url::Builder()
                .setScheme(nx::stun::kUrlSchemeName).setEndpoint(stunEndpoint()));
        network::SocketGlobals::cloud().mediatorConnector().enable(true);
    }

    return true;
}

SocketAddress MediatorFunctionalTest::stunEndpoint() const
{
    return SocketAddress(HostAddress::localhost, m_stunPort);
}

SocketAddress MediatorFunctionalTest::httpEndpoint() const
{
    return SocketAddress(HostAddress::localhost, m_httpPort);
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
                const boost::optional<nx::utils::Url>& /*cdbUrl*/,
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
    nx::String name, ServerTweak::Value tweak)
{
    auto server = std::make_unique<MediaServerEmulator>(
        stunEndpoint(),
        system,
        std::move(name));

    if (!server->start(!(tweak & ServerTweak::noListenToConnect)))
    {
        NX_LOGX(lm("Failed to start server: %1").arg(server->fullName()), cl_logERROR);
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

std::tuple<nx_http::StatusCode::Value, data::ListeningPeers>
    MediatorFunctionalTest::getListeningPeers() const
{
    nx_http::HttpClient httpClient;
    const auto urlStr =
        lm("http://%1%2").arg(httpEndpoint().toString())
        .arg(nx::hpm::http::GetListeningPeerListHandler::kHandlerPath);
    if (!httpClient.doGet(nx::utils::Url(urlStr)))
        return std::make_tuple(
            nx_http::StatusCode::serviceUnavailable,
            data::ListeningPeers());
    if (httpClient.response()->statusLine.statusCode != nx_http::StatusCode::ok)
        return std::make_tuple(
            static_cast<nx_http::StatusCode::Value>(
                httpClient.response()->statusLine.statusCode),
            data::ListeningPeers());

    QByteArray responseBody;
    while (!httpClient.eof())
        responseBody += httpClient.fetchMessageBodyBuffer();

    return std::make_tuple(
        nx_http::StatusCode::ok,
        QJson::deserialized<data::ListeningPeers>(responseBody));
}

void MediatorFunctionalTest::beforeModuleCreation()
{
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

    SocketAddress httpEndpoint = SocketAddress::anyPrivateAddress;
    if (m_httpPort != 0)
        httpEndpoint.port = m_httpPort;

    addArg("-stun/addrToListenList", m_stunAddress.toStdString().c_str());
    addArg("-http/addrToListenList", httpEndpoint.toStdString().c_str());
}

void MediatorFunctionalTest::afterModuleDestruction()
{
    //clearArgs();
}

} // namespace hpm
} // namespace nx
