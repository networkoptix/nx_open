#include <gtest/gtest.h>

#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

class AuthProvider: public CdbFunctionalTest {};

TEST_F(AuthProvider, nonce)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding system1 to account1
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(account1.email, account1Password, system1.id,
            account2.email, api::SystemAccessRole::liveViewer));

    api::NonceData nonceData;
    ASSERT_EQ(
        api::ResultCode::ok,
        getCdbNonce(account1.email, account1Password, system1.id, &nonceData));
    ASSERT_EQ(
        api::ResultCode::ok,
        getCdbNonce(account2.email, account2Password, system1.id, &nonceData));
}

} // namespace test
} // namespace cdb
} // namespace nx
