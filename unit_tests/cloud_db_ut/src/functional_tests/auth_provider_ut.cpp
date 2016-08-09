/**********************************************************
* Oct 19, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include "test_setup.h"


namespace nx {
namespace cdb {

class AuthProvider: public CdbFunctionalTest {};

TEST_F(AuthProvider, nonce)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    //adding system1 to account1
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    api::NonceData nonceData;
    for (int i = 0; i < 20; ++i)
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            getCdbNonce(system1.id, system1.authKey, &nonceData));
    }
}

}   //cdb
}   //nx
