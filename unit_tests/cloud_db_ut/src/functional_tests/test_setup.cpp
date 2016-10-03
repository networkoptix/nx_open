/**********************************************************
* Sep 29, 2015
* akolesnikov
***********************************************************/

#include "test_setup.h"

#include <gtest/gtest.h>

#include "email_manager_mocked.h"

/**
 * Analogue of gtest's ASSERT_EQ but supports placing in non-void methods (throws on failure).
 */
#define NX_GTEST_ASSERT_EQ(expected, actual) \
    do \
    { \
        bool result = false; \
        auto x = [&]() -> void \
        { \
            ASSERT_EQ(expected, actual); \
            result = true; \
        }; \
        x(); \
        if (!result) \
        { \
            throw std::runtime_error( \
                "This is work around ASSERT_* inability to be used " \
                "in non-void method. Just ignore..."); \
        } \
    } \
    while (0)

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
    api::AccountData account1;
    std::string account1Password;
    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        CdbLauncher::addActivatedAccount(&account1, &account1Password));

    return AccountWithPassword{ std::move(account1), std::move(account1Password) };
}

api::SystemData CdbFunctionalTest::addRandomSystemToAccount(
    const AccountWithPassword& account)
{
    //adding system to account
    api::SystemData system1;
    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        CdbLauncher::bindRandomSystem(account.data.email, account.password, &system1));
    return system1;
}

void CdbFunctionalTest::shareSystem2(
    const AccountWithPassword& from,
    const api::SystemData& what,
    const AccountWithPassword& to,
    api::SystemAccessRole targetRole)
{
    NX_GTEST_ASSERT_EQ(
        api::ResultCode::ok,
        CdbLauncher::shareSystem(
            from.data.email,
            from.password,
            what.id,
            to.data.email,
            targetRole));
}

} // namespace cdb
} // namespace nx
