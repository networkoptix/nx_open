/**********************************************************
* Sep 29, 2015
* akolesnikov
***********************************************************/

#include "test_setup.h"

#include <gtest/gtest.h>

#include <nx/utils/test_support/utils.h>

#include "email_manager_mocked.h"

namespace nx {
namespace cdb {

CdbFunctionalTest::CdbFunctionalTest()
{
}

CdbFunctionalTest::~CdbFunctionalTest()
{
    stop();
}

AccountWithPassword CdbFunctionalTest::addActivatedAccount2()
{
    //creating two accounts
    AccountWithPassword account;
    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        CdbLauncher::addActivatedAccount(&account, &account.password));
    return account;
}

api::SystemData CdbFunctionalTest::addRandomSystemToAccount(
    const AccountWithPassword& account)
{
    return addRandomSystemToAccount(account, api::SystemData());
}

api::SystemData CdbFunctionalTest::addRandomSystemToAccount(
    const AccountWithPassword& account,
    const api::SystemData& systemPrototype)
{
    api::SystemData system1 = systemPrototype;
    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        CdbLauncher::bindRandomSystem(account.email, account.password, &system1));
    return system1;
}

void CdbFunctionalTest::shareSystemEx(
    const AccountWithPassword& from,
    const api::SystemData& what,
    const AccountWithPassword& to,
    api::SystemAccessRole targetRole)
{
    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        CdbLauncher::shareSystem(
            from.email,
            from.password,
            what.id,
            to.email,
            targetRole));
}

void CdbFunctionalTest::shareSystemEx(
    const AccountWithPassword& from,
    const api::SystemData& what,
    const std::string& emailToShareWith,
    api::SystemAccessRole targetRole)
{
    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        CdbLauncher::shareSystem(
            from.email,
            from.password,
            what.id,
            emailToShareWith,
            targetRole));
}

void CdbFunctionalTest::enableUser(
    const AccountWithPassword& who,
    const api::SystemData& what,
    const AccountWithPassword& whom)
{
    setUserEnabledFlag(who, what, whom, true);
}

void CdbFunctionalTest::disableUser(
    const AccountWithPassword& who,
    const api::SystemData& what,
    const AccountWithPassword& whom)
{
    setUserEnabledFlag(who, what, whom, false);
}

void CdbFunctionalTest::setUserEnabledFlag(
    const AccountWithPassword& who,
    const api::SystemData& what,
    const AccountWithPassword& whom,
    bool isEnabled)
{
    api::SystemSharingEx userData;
    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        getSystemSharing(who.email, who.password, what.id, whom.email, &userData));

    userData.isEnabled = isEnabled;

    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(who.email, who.password, userData));
}

} // namespace cdb
} // namespace nx
