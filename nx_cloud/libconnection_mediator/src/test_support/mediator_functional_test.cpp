/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#include "mediator_functional_test.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
#include <tuple>

#include <common/common_globals.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/socket.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <utils/common/sync_call.h>
#include <utils/crypt/linux_passwd_crypt.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>

#include "http/get_listening_peer_list_handler.h"
#include "local_cloud_data_provider.h"
#include "mediator_service.h"


namespace nx {
namespace hpm {

MediatorFunctionalTest::MediatorFunctionalTest()
:
    m_stunPort(0),
    m_httpPort(0)
{
    //starting clean test
    nx::network::SocketGlobalsHolder::instance()->reinitialize();

    m_tmpDir = QDir::homePath() + "/hpm_ut.data";
    QDir(m_tmpDir).removeRecursively();

    addArg("/path/to/bin");
    addArg("-e");
    addArg("-stun/addrToListenList"); addArg("127.0.0.1:0");
    addArg("-http/addrToListenList"); addArg("127.0.0.1:0");
    addArg("-log/logLevel"); addArg("none");
    addArg("-general/dataDir"); addArg(m_tmpDir.toLatin1().constData());

    registerCloudDataProvider(&m_cloudDataProvider);
}

MediatorFunctionalTest::~MediatorFunctionalTest()
{
    stop();

    QDir(m_tmpDir).removeRecursively();
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

    network::SocketGlobals::mediatorConnector().mockupAddress(stunEndpoint());
    network::SocketGlobals::mediatorConnector().enable(true);

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

std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection>
    MediatorFunctionalTest::clientConnection()
{
    return network::SocketGlobals::mediatorConnector().clientConnection();
}

std::shared_ptr<nx::hpm::api::MediatorServerTcpConnection>
    MediatorFunctionalTest::systemConnection()
{
    return network::SocketGlobals::mediatorConnector().systemConnection();
}

void MediatorFunctionalTest::registerCloudDataProvider(
    AbstractCloudDataProvider* cloudDataProvider)
{
    AbstractCloudDataProviderFactory::setFactoryFunc(
        [cloudDataProvider](
            const std::string& /*address*/,
            const std::string& /*user*/,
            const std::string& /*password*/,
            std::chrono::milliseconds /*updateInterval*/)
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
    nx::String name, bool bindEndpoint)
{
    auto server = std::make_unique<MediaServerEmulator>(
        stunEndpoint(),
        system,
        std::move(name));

    if (!server->start())
    {
        NX_LOGX(lm("Failed to start server: %1").arg(server->fullName()), cl_logERROR);
        return nullptr;
    }

    if (bindEndpoint && server->bind() != nx::hpm::api::ResultCode::ok)
    {
        NX_LOGX(lm("Failed to bind server: %1, endpoint=%2")
            .arg(server->fullName()).str(server->endpoint()), cl_logERROR);
        return nullptr;
    }

    return server;
}

std::unique_ptr<MediaServerEmulator> MediatorFunctionalTest::addRandomServer(
    const AbstractCloudDataProvider::System& system, bool bindEndpoint)
{
    return addServer(system, QnUuid::createUuid().toSimpleString().toUtf8(), bindEndpoint);
}

std::vector<std::unique_ptr<MediaServerEmulator>>
    MediatorFunctionalTest::addRandomServers(
        const AbstractCloudDataProvider::System& system,
        size_t count, bool bindEndpoint)
{
    std::vector<std::unique_ptr<MediaServerEmulator>> systemServers;
    for (size_t i = 0; i < count; ++i)
    {
        auto server = addRandomServer(system, bindEndpoint);
        if (!server)
            return {};

        systemServers.push_back(std::move(server));
    }

    return systemServers;
}

std::tuple<nx_http::StatusCode::Value, data::ListeningPeersBySystem>
    MediatorFunctionalTest::getListeningPeers() const
{
    nx_http::HttpClient httpClient;
    const auto urlStr =
        lm("http://%1%2").arg(httpEndpoint().toString())
        .arg(nx::hpm::http::GetListeningPeerListHandler::kHandlerPath);
    if (!httpClient.doGet(QUrl(urlStr)))
        return std::make_tuple(
            nx_http::StatusCode::serviceUnavailable,
            data::ListeningPeersBySystem());
    if (httpClient.response()->statusLine.statusCode != nx_http::StatusCode::ok)
        return std::make_tuple(
            static_cast<nx_http::StatusCode::Value>(
                httpClient.response()->statusLine.statusCode),
            data::ListeningPeersBySystem());

    QByteArray responseBody;
    while (!httpClient.eof())
        responseBody += httpClient.fetchMessageBodyBuffer();

    return std::make_tuple(
        nx_http::StatusCode::ok,
        QJson::deserialized<data::ListeningPeersBySystem>(responseBody));
}

}   // namespace hpm
}   // namespace nx
