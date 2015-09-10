/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <chrono>
#include <functional>
#include <future>
#include <thread>
#include <tuple>

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <utils/common/cpp14.h>
#include <utils/network/http/auth_tools.h>

#include <cdb/connection.h>
#include <cdb/account_manager.h>
#include <cloud_db_process.h>

#include "version.h"


namespace nx {
namespace cdb {
namespace cl {


template<typename ResultType>
std::tuple<ResultType> makeSyncCall(
    std::function<void(std::function<void(ResultType)>)> function)
{
    std::promise<ResultType> promise;
    auto future = promise.get_future();
    function([&promise](ResultType resCode){ promise.set_value(resCode); });
    future.wait();
    return std::make_tuple(future.get());
}

template<typename ResultType, typename OutArg1>
std::tuple<ResultType, OutArg1> makeSyncCall(
    std::function<void(std::function<void(ResultType, OutArg1)>)> function)
{
    std::promise<ResultType> promise;
    auto future = promise.get_future();
    OutArg1 result;
    function([&promise, &result](ResultType resCode, OutArg1 outArg1) {
        result = std::move(outArg1);
        promise.set_value(resCode);
    });
    future.wait();
    return std::make_tuple(future.get(), std::move(result));
}


class CdbFunctionalTest
:
    public ::testing::Test
{
public:
    CdbFunctionalTest()
    :
        m_port(0),
        m_connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
    {
        m_port = (std::rand() % 10000) + 50000;
        m_tmpDir = QDir::homePath() + "/cdb_ut.data";
        QDir(m_tmpDir).removeRecursively();

        auto b = std::back_inserter(m_args);
        *b = strdup("/path/to/bin");
        *b = strdup("-e");
        *b = strdup("-listenOn"); *b = strdup(lit("0.0.0.0:%1").arg(m_port).toLatin1().constData());
        *b = strdup("-dataDir"); *b = strdup(m_tmpDir.toLatin1().constData());
        *b = strdup("-db/driverName"); *b = strdup("QSQLITE");
        *b = strdup("-db/name"); *b = strdup(lit("%1/%2").arg(m_tmpDir).arg(lit("cdb_ut.sqlite")).toLatin1().constData());

        m_connectionFactory->setCloudEndpoint("127.0.0.1", m_port);

        m_cdbInstance = std::make_unique<nx::cdb::CloudDBProcess>(
            static_cast<int>(m_args.size()), m_args.data());

        m_cdbProcessFuture = std::async(
            std::launch::async,
            std::bind(&CloudDBProcess::exec, m_cdbInstance.get()));
    }

    ~CdbFunctionalTest()
    {
        m_cdbInstance->pleaseStop();
        m_cdbProcessFuture.wait();
        m_cdbInstance.reset();

        QDir(m_tmpDir).removeRecursively();
    }

    void waitUntilStarted()
    {
        static const std::chrono::seconds CDB_INITIALIZED_MAX_WAIT_TIME(15);

        const auto endClock = std::chrono::steady_clock::now() + CDB_INITIALIZED_MAX_WAIT_TIME;
        for (;;)
        {
            ASSERT_TRUE(std::chrono::steady_clock::now() < endClock);

            auto connection = m_connectionFactory->createConnection("", "");
            api::ResultCode result = api::ResultCode::ok;
            std::tie(result, m_moduleInfo) = makeSyncCall<api::ResultCode, api::ModuleInfo>(
                std::bind(
                    &nx::cdb::api::Connection::ping,
                    connection.get(),
                    std::placeholders::_1));
            if (result == api::ResultCode::ok)
                break;
        }
    }

protected:
    nx::cdb::api::ConnectionFactory* connectionFactory()
    {
        return m_connectionFactory.get();
    }

    api::ModuleInfo moduleInfo() const
    {
        return m_moduleInfo;
    }

private:
    QString m_tmpDir;
    int m_port;
    std::vector<char*> m_args;
    std::unique_ptr<CloudDBProcess> m_cdbInstance;
    std::future<int> m_cdbProcessFuture;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)
    > m_connectionFactory;
    api::ModuleInfo m_moduleInfo;
};


TEST_F(CdbFunctionalTest, account)
{
    //waiting for cloud_db initialization
    waitUntilStarted();

    //initializing connection
    {
        auto connection = connectionFactory()->createConnection( "", "" );

        //adding account
        nx::cdb::api::AccountData accountData;
        accountData.login = "test";
        accountData.email = "test@networkoptix.com";
        accountData.fullName = "Test Test";
        accountData.passwordHa1 = nx_http::calcHa1(
            lit("test"),
            QLatin1String(moduleInfo().realm.c_str()),
            lit("123"));
        api::ResultCode result = api::ResultCode::ok;
        std::tie(result) = makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::AccountManager::registerNewAccount,
                connection->accountManager(),
                std::move(accountData),
                std::placeholders::_1));
        ASSERT_EQ(result, api::ResultCode::ok);
    }

    //fetching account
    {
        auto connection = connectionFactory()->createConnection("test", "123");

        api::ResultCode resCode = api::ResultCode::ok;
        nx::cdb::api::AccountData accountData;
        std::tie(resCode, accountData) =
            makeSyncCall<api::ResultCode, nx::cdb::api::AccountData>(
                std::bind(
                    &nx::cdb::api::AccountManager::getAccount,
                    connection->accountManager(),
                    std::placeholders::_1));
        ASSERT_EQ(resCode, api::ResultCode::ok);
        ASSERT_EQ(accountData.login, "test");
    }
}

}   //cl
}   //cdb
}   //nx
