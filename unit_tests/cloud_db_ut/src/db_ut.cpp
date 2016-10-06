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
    if (isStartedWithExternalDb())
        return; //< Ignoring.
    ASSERT_TRUE(placePreparedDB(":/cdb.sqlite"));

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
    ASSERT_EQ(
        api::ResultCode::ok,
        getAccount("akolesnikov@networkoptix.com", "123", &testAccount));
    ASSERT_EQ("Andrey Kolesnikov", testAccount.fullName);

    std::vector<api::SystemDataEx> systems;
    ASSERT_EQ(api::ResultCode::ok, getSystems("akolesnikov@networkoptix.com", "123", &systems));
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

} // namespace test
} // namespace cdb
} // namespace nx
