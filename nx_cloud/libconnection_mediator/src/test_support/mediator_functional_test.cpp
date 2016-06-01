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
#include <utils/common/cpp14.h>
#include <utils/common/string.h>
#include <utils/common/sync_call.h>
#include <utils/crypt/linux_passwd_crypt.h>
#include <utils/serialization/json.h>
#include <utils/serialization/lexical.h>

#include "http/get_listening_peer_list_handler.h"
#include "local_cloud_data_provider.h"


namespace nx {
namespace hpm {

MediatorFunctionalTest::MediatorFunctionalTest()
:
    m_stunPort(0),
    m_httpPort(0)
{
    //starting clean test
    nx::network::SocketGlobalsHolder::instance()->reinitialize();

    m_stunPort = (std::rand() % 10000) + 50000;
    m_httpPort = m_stunPort+1;
    m_tmpDir = QDir::homePath() + "/hpm_ut.data";
    QDir(m_tmpDir).removeRecursively();

    addArg("/path/to/bin");
    addArg("-e");
    addArg("-stun/addrToListenList"); addArg(lit("127.0.0.1:%1").arg(m_stunPort).toLatin1().constData());
    addArg("-http/addrToListenList"); addArg(lit("127.0.0.1:%1").arg(m_httpPort).toLatin1().constData());
    //addArg("-log/logLevel"); addArg("DEBUG2");
    addArg("-general/dataDir"); addArg(m_tmpDir.toLatin1().constData());

    network::SocketGlobals::mediatorConnector().mockupAddress(stunEndpoint());
    registerCloudDataProvider(&m_cloudDataProvider);
}

MediatorFunctionalTest::~MediatorFunctionalTest()
{
    stop();

    QDir(m_tmpDir).removeRecursively();
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
            const std::string& address,
            const std::string& user,
            const std::string& password,
            std::chrono::milliseconds updateInterval)
    {
        return std::make_unique<CloudDataProviderStub>(cloudDataProvider);
    });
}

AbstractCloudDataProvider::System MediatorFunctionalTest::addRandomSystem()
{
    AbstractCloudDataProvider::System system(
        generateRandomName(16),
        generateRandomName(16),
        true);
    m_cloudDataProvider.addSystem(
        system.id,
        system);
    return system;
}

std::unique_ptr<MediaServerEmulator> MediatorFunctionalTest::addServer(
    const AbstractCloudDataProvider::System& system,
    nx::String name)
{
    auto server = std::make_unique<MediaServerEmulator>(
        stunEndpoint(),
        system,
        std::move(name));
    if (!server->start() || (server->registerOnMediator() != api::ResultCode::ok))
        return nullptr;
    return server;
}

std::unique_ptr<MediaServerEmulator> MediatorFunctionalTest::addRandomServer(
    const AbstractCloudDataProvider::System& system)
{
    auto server = std::make_unique<MediaServerEmulator>(stunEndpoint(), system);
    if (!server->start())
    {
        std::cerr<<"Failed to start server"<<std::endl;
        return nullptr;
    }
    const auto resultCode = server->registerOnMediator();
    if (resultCode != api::ResultCode::ok)
    {
        std::cerr<<"Failed to register server on mediator. "
            <<QnLexical::serialized(resultCode).toStdString()<<std::endl;
        return nullptr;
    }
    return server;
}

std::unique_ptr<MediaServerEmulator>
    MediatorFunctionalTest::addRandomServerNotRegisteredOnMediator(
        const AbstractCloudDataProvider::System& system)
{
    auto server = std::make_unique<MediaServerEmulator>(stunEndpoint(), system);
    if (!server->start())
        return nullptr;
    return server;
}

std::vector<std::unique_ptr<MediaServerEmulator>>
    MediatorFunctionalTest::addRandomServers(
        const AbstractCloudDataProvider::System& system,
        size_t count)
{
    std::vector<std::unique_ptr<MediaServerEmulator>> systemServers;
    systemServers.push_back(std::make_unique<MediaServerEmulator>(stunEndpoint(), system));
    systemServers.push_back(std::make_unique<MediaServerEmulator>(stunEndpoint(), system));
    for (auto& server: systemServers)
        if (!server->start() || (server->registerOnMediator() != api::ResultCode::ok))
            return std::vector<std::unique_ptr<MediaServerEmulator>>();
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
