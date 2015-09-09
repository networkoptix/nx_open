/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <functional>
#include <future>
#include <thread>
#include <tuple>

#include <gtest/gtest.h>

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


static char* argv[] = {
    "D:/develop/netoptix_vms.nat_traversal/build_environment/target/x64/bin/debug/cloud_db_ut.exe",
    "-e",
    "--cloudDbUrl=http://10.0.2.95:3346/",
    "--dataDir=C:/cloud_db_ut.data/",
    "--db/driverName=QSQLITE",
    "--db/name=cloud_db_ut.sqlite"
};
static const auto argc = sizeof(argv) / sizeof(*argv);

class CdbFunctionalTest
:
    public ::testing::Test
{
public:
    CdbFunctionalTest()
    {
        QFile::remove("cloud_db_ut.sqlite");
        m_cdbInstance = std::make_unique<nx::cdb::CloudDBProcess>(argc, argv);

        m_cdbProcessFuture = std::async(
            std::launch::async,
            std::bind(&CloudDBProcess::exec, m_cdbInstance.get()));
    }

    ~CdbFunctionalTest()
    {
        m_cdbInstance->pleaseStop();
        m_cdbInstance.reset();
        m_cdbProcessFuture.wait();

        QFile::remove("cloud_db_ut.sqlite");
    }

private:
    std::unique_ptr<CloudDBProcess> m_cdbInstance;
    std::future<int> m_cdbProcessFuture;
};


api::ResultCode makeSyncCall(std::function<void(std::function<void(api::ResultCode)>)> function)
{
    std::promise<api::ResultCode> promise;
    auto future = promise.get_future();
    function([&promise](api::ResultCode resCode){ promise.set_value(resCode); });
    future.wait();
    return future.get();
}

template<typename OutArg1>
std::tuple<api::ResultCode, OutArg1> makeSyncCall1(std::function<void(std::function<void(api::ResultCode, OutArg1)>)> function)
{
    std::promise<api::ResultCode> promise;
    auto future = promise.get_future();
    OutArg1 result;
    function([&promise, &result](api::ResultCode resCode, OutArg1 outArg1) {
        result = std::move(outArg1);
        promise.set_value(resCode);
    });
    future.wait();
    return std::make_tuple(future.get(), std::move(result));
}



TEST_F(CdbFunctionalTest, account)
{
    //waiting for initialization
    std::this_thread::sleep_for(std::chrono::seconds(5));

    //initializing connection
    std::unique_ptr<nx::cdb::api::ConnectionFactory, decltype(&destroyConnectionFactory)>
        connectionFactory(createConnectionFactory(), &destroyConnectionFactory);
    {
        auto connection = connectionFactory->createConnection( "", "" );

        //adding account
        nx::cdb::api::AccountData accountData;
        accountData.login = "test";
        accountData.email = "test@networkoptix.com";
        accountData.fullName = "Test Test";
        accountData.passwordHa1 = nx_http::calcHa1(
            lit("test"),
            QLatin1String(QN_REALM),
            lit("123"));
        auto result = makeSyncCall(
            std::bind(
                &nx::cdb::api::AccountManager::registerNewAccount,
                connection->accountManager(),
                std::move(accountData),
                std::placeholders::_1));
        ASSERT_EQ(result, api::ResultCode::ok);
    }

    //fetching account
    {
        auto connection = connectionFactory->createConnection("test", "123");

        api::ResultCode resCode = api::ResultCode::ok;
        nx::cdb::api::AccountData accountData;
        std::tie(resCode, accountData) = makeSyncCall1<nx::cdb::api::AccountData>(
            std::bind(
                &nx::cdb::api::AccountManager::getAccount,
                connection->accountManager(),
                std::placeholders::_1));
        ASSERT_EQ(resCode, api::ResultCode::ok);
        ASSERT_EQ(accountData.login, "test");
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
}

}   //cl
}   //cdb
}   //nx
