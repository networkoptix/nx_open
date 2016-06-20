/**********************************************************
* Jul 17, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QtCore/QDir>

#include "functional_tests/test_setup.h"


namespace nx {
namespace cdb {
namespace test {

class DbRegress
:
    public CdbFunctionalTest
{
public:
    DbRegress()
    {
        QDir().mkpath(testDataDir());
    }
};

TEST_F(DbRegress, general)
{
    //starting with old db
    const nx::db::ConnectionOptions connectionOptions = dbConnectionOptions();
    if (!connectionOptions.dbName.isEmpty())
        return; //test is started with external DB: ignoring

    const QString dbPath = QDir::cleanPath(testDataDir() + "/cdb_ut.sqlite");
    QDir().remove(dbPath);
    ASSERT_TRUE(QFile::copy(":/cdb.sqlite", dbPath));
    ASSERT_TRUE(QFile(dbPath).setPermissions(
        QFileDevice::ReadOwner | QFileDevice::WriteOwner |
        QFileDevice::ReadUser | QFileDevice::WriteUser));
    
    ASSERT_TRUE(startAndWaitUntilStarted());

    //checking that cdb has started
    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);
    
    //adding system1 to account1
    api::SystemData system1;
    result = bindRandomSystem(account1.email, account1Password, &system1);
    ASSERT_EQ(api::ResultCode::ok, result);

    api::AccountData account1Tmp;
    result = getAccount(account1.email, account1Password, &account1Tmp);
    ASSERT_EQ(api::ResultCode::ok, result);

    //checking that data is there
    api::AccountData testAccount;
    result = getAccount("akolesnikov@networkoptix.com", "123", &testAccount);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ("Andrey Kolesnikov", testAccount.fullName);

    std::vector<api::SystemDataEx> systems;
    result = getSystems("akolesnikov@networkoptix.com", "123", &systems);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(6, systems.size());

    const auto laOfficeTestSystemIter = std::find_if(
        systems.begin(), systems.end(),
        [](api::SystemDataEx& systemData)
        {
            return systemData.name == "la_office_test";
        });
    ASSERT_NE(systems.end(), laOfficeTestSystemIter);
    ASSERT_EQ(api::SystemStatus::ssActivated, laOfficeTestSystemIter->status);
}

}   //test
}   //cdb
}   //nx
