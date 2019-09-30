#include <future>

#include <gtest/gtest.h>

#include "functional_tests/test_setup.h"

namespace nx::cloud::db {
namespace test {

TEST_F(CdbFunctionalTest, ping)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    auto connection = connectionFactory()->createConnection("", "");
    std::promise<std::tuple<api::ResultCode, api::ModuleInfo>> done;
    connection->ping(
        [&done](api::ResultCode resultCode, api::ModuleInfo moduleInfo)
        {
            done.set_value(std::make_tuple(resultCode, std::move(moduleInfo)));
        });
    auto [result, moduleInfo] = done.get_future().get();
    ASSERT_EQ(api::ResultCode::ok, result);
}

TEST_F(CdbFunctionalTest, client_cancellation)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    //adding system to account1
    api::SystemData system0;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system0));

    for (std::atomic<int> seq(0); seq < 100; ++seq)
    {
        auto connection = connectionFactory()->createConnection(
            system0.id, system0.authKey);
        connection->ping(
            [seqCopy = seq.load(), &seq](api::ResultCode, api::ModuleInfo)
            {
                ASSERT_EQ(seqCopy, seq.load());
            });
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
}

} // namespace test
} // namespace nx::cloud::db
