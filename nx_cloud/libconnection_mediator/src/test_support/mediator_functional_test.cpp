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
#include <nx/network/socket.h>
#include <utils/common/cpp14.h>
#include <utils/common/string.h>
#include <utils/common/sync_call.h>
#include <utils/crypt/linux_passwd_crypt.h>
#include <utils/serialization/lexical.h>

#include "local_cloud_data_provider.h"
#include "socket_globals_holder.h"

namespace nx {
namespace hpm {

MediatorFunctionalTest::MediatorFunctionalTest()
:
    m_port(0)
{
    //starting clean test
    SocketGlobalsHolder::instance()->reinitialize();

    m_port = (std::rand() % 10000) + 50000;
    m_tmpDir = QDir::homePath() + "/hpm_ut.data";
    QDir(m_tmpDir).removeRecursively();

    auto b = std::back_inserter(m_args);
    *b = strdup("/path/to/bin");
    *b = strdup("-e");
    *b = strdup("-stun/addrToListenList"); *b = strdup(lit("127.0.0.1:%1").arg(m_port).toLatin1().constData());
    *b = strdup("-log/logLevel"); *b = strdup("DEBUG2");
    *b = strdup("-dataDir"); *b = strdup(m_tmpDir.toLatin1().constData());

    network::SocketGlobals::mediatorConnector().mockupAddress(endpoint());
    registerCloudDataProvider(&m_cloudDataProvider);
}

MediatorFunctionalTest::~MediatorFunctionalTest()
{
    stop();

    QDir(m_tmpDir).removeRecursively();
}

void MediatorFunctionalTest::start()
{
    nx::utils::promise<void> mediatorInstantiatedCreatedPromise;
    auto mediatorInstantiatedCreatedFuture = mediatorInstantiatedCreatedPromise.get_future();

    m_mediatorProcessThread = nx::utils::thread(
        [this, &mediatorInstantiatedCreatedPromise]()->int {
            m_mediatorInstance = std::make_unique<nx::hpm::MediatorProcessPublic>(
                static_cast<int>(m_args.size()), m_args.data());
            m_mediatorInstance->setOnStartedEventHandler(
                [this](bool result)
                {
                    m_mediatorStartedPromise.set_value(result);
                });
            mediatorInstantiatedCreatedPromise.set_value();
            return m_mediatorInstance->exec();
        });
    mediatorInstantiatedCreatedFuture.wait();
}

bool MediatorFunctionalTest::startAndWaitUntilStarted()
{
    start();
    return waitUntilStarted();
}

bool MediatorFunctionalTest::waitUntilStarted()
{
    static const std::chrono::seconds initializedMaxWaitTime(15);

    auto mediatorStartedFuture = m_mediatorStartedPromise.get_future();
    if (mediatorStartedFuture.wait_for(initializedMaxWaitTime) !=
            std::future_status::ready)
    {
        return false;
    }

    return mediatorStartedFuture.get();
}

void MediatorFunctionalTest::stop()
{
    m_mediatorInstance->pleaseStop();
    m_mediatorProcessThread.join();
    m_mediatorInstance.reset();
}

void MediatorFunctionalTest::restart()
{
    stop();
    start();
    waitUntilStarted();
}

void MediatorFunctionalTest::addArg(const char* arg)
{
    auto b = std::back_inserter(m_args);
    *b = strdup(arg);
}

SocketAddress MediatorFunctionalTest::endpoint() const
{
    return SocketAddress(HostAddress::localhost, m_port);
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
        endpoint(),
        system,
        std::move(name));
    if (!server->start() || (server->registerOnMediator() != api::ResultCode::ok))
        return nullptr;
    return server;
}

std::unique_ptr<MediaServerEmulator> MediatorFunctionalTest::addRandomServer(
    const AbstractCloudDataProvider::System& system)
{
    auto server = std::make_unique<MediaServerEmulator>(endpoint(), system);
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
    auto server = std::make_unique<MediaServerEmulator>(endpoint(), system);
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
    systemServers.push_back(std::make_unique<MediaServerEmulator>(endpoint(), system));
    systemServers.push_back(std::make_unique<MediaServerEmulator>(endpoint(), system));
    for (auto& server: systemServers)
        if (!server->start() || (server->registerOnMediator() != api::ResultCode::ok))
            return std::vector<std::unique_ptr<MediaServerEmulator>>();
    return systemServers;
}

//api::ResultCode MediatorFunctionalTest::addAccount(
//    api::AccountData* const accountData,
//    std::string* const password,
//    api::AccountConfirmationCode* const activationCode)
//{
//    if (accountData->email.empty())
//    {
//        std::ostringstream ss;
//        ss << "test_" << rand() << "@networkoptix.com";
//        accountData->email = ss.str();
//    }
//
//    if (password->empty())
//    {
//        std::ostringstream ss;
//        ss << rand();
//        *password = ss.str();
//    }
//
//    if (accountData->fullName.empty())
//        accountData->fullName = "Test Test";
//    if (accountData->passwordHa1.empty())
//        accountData->passwordHa1 = nx_http::calcHa1(
//            QUrl::fromPercentEncoding(QByteArray(accountData->email.c_str())).toLatin1().constData(),
//            //accountData->email.c_str(),
//            moduleInfo().realm.c_str(),
//            password->c_str()).constData();
//    if (accountData->customization.empty())
//        accountData->customization = QnAppInfo::customizationName();
//
//    auto connection = connectionFactory()->createConnection("", "");
//
//    //adding account
//    api::ResultCode result = api::ResultCode::ok;
//    std::tie(result, *activationCode) = makeSyncCall<api::ResultCode, api::AccountConfirmationCode>(
//        std::bind(
//            &nx::cdb::api::AccountManager::registerNewAccount,
//            connection->accountManager(),
//            *accountData,
//            std::placeholders::_1));
//    return result;
//}

}   //hpm
}   //nx
