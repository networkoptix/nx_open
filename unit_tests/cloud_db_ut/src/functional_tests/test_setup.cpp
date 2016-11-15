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
    //adding system to account
    api::SystemData system1;
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

} // namespace cdb
} // namespace nx
