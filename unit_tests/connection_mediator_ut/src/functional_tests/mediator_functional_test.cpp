/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#include "mediator_functional_test.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <future>
#include <sstream>
#include <thread>
#include <tuple>

#include <common/common_globals.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket.h>
#include <utils/common/cpp14.h>
#include <utils/common/sync_call.h>
#include <utils/crypt/linux_passwd_crypt.h>

#include "local_cloud_data_provider.h"
#include "socket_globals_holder.h"
#include "version.h"


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
    std::promise<void> mediatorInstantiatedCreatedPromise;
    auto mediatorInstantiatedCreatedFuture = mediatorInstantiatedCreatedPromise.get_future();

    m_mediatorProcessFuture = std::async(
        std::launch::async,
        [this, &mediatorInstantiatedCreatedPromise]()->int {
            m_mediatorInstance = std::make_unique<nx::hpm::MediatorProcessPublic>(
                static_cast<int>(m_args.size()), m_args.data());
            mediatorInstantiatedCreatedPromise.set_value();
            return m_mediatorInstance->exec();
        });
    mediatorInstantiatedCreatedFuture.wait();
}

void MediatorFunctionalTest::startAndWaitUntilStarted()
{
    start();
    waitUntilStarted();
}

void MediatorFunctionalTest::waitUntilStarted()
{
    static const std::chrono::seconds INITIALIZED_MAX_WAIT_TIME(15);

    const auto endClock = std::chrono::steady_clock::now() + INITIALIZED_MAX_WAIT_TIME;
    for (;;)
    {
        ASSERT_TRUE(std::chrono::steady_clock::now() < endClock);

        ASSERT_NE(
            std::future_status::ready,
            m_mediatorProcessFuture.wait_for(std::chrono::seconds(0)));

        auto socket = SocketFactory::createStreamSocket(false, SocketFactory::NatTraversalType::nttDisabled);
        if (socket->connect(endpoint(), INITIALIZED_MAX_WAIT_TIME/5))
            break;
    }
}

void MediatorFunctionalTest::stop()
{
    m_mediatorInstance->pleaseStop();
    m_mediatorProcessFuture.wait();
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

std::shared_ptr<nx::network::cloud::MediatorClientConnection> 
    MediatorFunctionalTest::clientConnection()
{
    return network::SocketGlobals::mediatorConnector().clientConnection();
}

std::shared_ptr<nx::network::cloud::MediatorSystemConnection>
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
            TimerDuration updateInterval)
    {
        return std::make_unique<CloudDataProviderStub>(cloudDataProvider);
    });
}

AbstractCloudDataProvider::System MediatorFunctionalTest::addRandomSystem()
{
    AbstractCloudDataProvider::System system(
        generateSalt(16),
        generateSalt(16),
        true);
    m_cloudDataProvider.addSystem(
        system.id,
        system);
    return system;
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
//        accountData->customization = QN_CUSTOMIZATION_NAME;
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

