#include <future>

#include <gtest/gtest.h>

#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/std/algorithm.h>

#include <test_support/appserver2_process.h>

namespace ec2::test {

class UserManagement:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    UserManagement():
        nx::utils::test::TestWithTemporaryDirectory("appserver2_ut", "")
    {
        const auto dbFileArg =
            lm("--dbFile=%1/db.sqlite").args(testDataDir()).toStdString();
        m_process.addArg(dbFileArg.c_str());
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_process.startAndWaitUntilStarted());
    }

    void whenAddUser()
    {
        m_user.id = QnUuid::createUuid();
        m_user.name = "test";
        m_userPassword = "passwd";

        std::promise<ec2::ErrorCode> done;
        m_process.moduleInstance()->ecConnection()->getUserManager(Qn::kSystemAccess)->save(
            m_user,
            m_userPassword,
            [&done](ec2::ErrorCode errorCode) { done.set_value(errorCode); });

        ASSERT_EQ(ec2::ErrorCode::ok, done.get_future().get());
    }

    void thenUserIsAdded()
    {
        std::promise<std::tuple<ec2::ErrorCode, nx::vms::api::UserDataList>> done;
        m_process.moduleInstance()->ecConnection()->getUserManager(Qn::kSystemAccess)->getUsers(
            [&done](auto&&... args) { done.set_value(std::make_tuple(std::forward<decltype(args)>(args)...)); });

        const auto [resultCode, users] = done.get_future().get();
        ASSERT_EQ(ec2::ErrorCode::ok, resultCode);

        ASSERT_TRUE(nx::utils::contains_if(users.begin(), users.end(),
            [this](const auto& user) { return user.id == m_user.id; }));
    }

private:
    nx::utils::test::ModuleLauncher<Appserver2Process> m_process;
    nx::vms::api::UserData m_user;
    QString m_userPassword;
};

TEST_F(UserManagement, add_user)
{
    whenAddUser();
    thenUserIsAdded();
}

} // namespace ec2::test
