#include <memory>
#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/user_data_ex.h>
#include <nx/vms/api/data/user_data.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"

namespace nx::test {

using namespace network;

class SaveUserEx: public ::testing::Test
{
protected:
    using LauncherPtr = std::unique_ptr<MediaServerLauncher>;

    vms::api::UserDataEx regularUser1;
    vms::api::UserDataEx regularUser2;
    vms::api::UserDataEx newAdminUser;
    vms::api::UserDataEx defaultAdmin;

    virtual void SetUp() override
    {
        initUserData();
        m_server = std::make_unique<MediaServerLauncher>();
        m_server->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        ASSERT_TRUE(m_server->start());
    }

    void whenSaveUserRequestIssued(const vms::api::UserDataEx& accessUser,
        const vms::api::UserDataEx& userToSave,
        http::StatusCode::Value expectedCode)
    {
        NX_TEST_API_POST(m_server.get(), "/ec2/saveUser", userToSave, nullptr,
            expectedCode, accessUser.name.toLower(), accessUser.password, &m_responseBuffer);
        NX_VERBOSE(this, lm("/ex2/saveUser issued successfully, response: %1").args(m_responseBuffer));
    }

    void whenChangePasswordRequestIssued(const vms::api::UserDataEx& accessUser,
        vms::api::UserDataEx* userToSave,
        const QString& newPassword,
        http::StatusCode::Value expectedCode)
    {
        const auto accessPassword = accessUser.password;
        userToSave->password = newPassword;
        NX_TEST_API_POST(m_server.get(), "/ec2/saveUser", *userToSave, nullptr,
            expectedCode, accessUser.name.toLower(), accessPassword, &m_responseBuffer);
        NX_VERBOSE(this, lm("/ex2/saveUser issued successfully, response: %1").args(m_responseBuffer));
    }

    void thenUserShouldAppearInTheGetUsersResponse(const vms::api::UserDataEx& expectedUser)
    {
        NX_TEST_API_GET(m_server.get(), "/ec2/getUsers", &m_userDataList);
        auto testUserIt = std::find_if(m_userDataList.cbegin(), m_userDataList.cend(),
            [&expectedUser](const auto& userData) { return userData.name == expectedUser.name; });
        ASSERT_NE(testUserIt, m_userDataList.cend());
    }

    void thenSavedUserShouldBeAuthorizedByServer(const vms::api::UserDataEx& accessUser)
    {
        NX_TEST_API_GET(m_server.get(), "/ec2/getUsers", &m_userDataList,
            http::StatusCode::ok, accessUser.name.toLower(), accessUser.password);
    }

    void whenUserNameChanged(vms::api::UserDataEx* userData, const QString& newName)
    {
        userData->name = newName;
    }

    void whenIdFilled(vms::api::UserDataEx* userData)
    {
        for (const auto& existingUser: m_userDataList)
        {
            if (existingUser.name == userData->name)
                userData->id = existingUser.id;
        }
        ASSERT_FALSE(userData->id.isNull());
    }

private:
    LauncherPtr m_server;
    vms::api::UserDataList m_userDataList;
    Buffer m_responseBuffer;

    void initUserData()
    {
        regularUser1.email = "test@test.com";
        regularUser1.fullName = "Adv";
        regularUser1.isEnabled = true;
        regularUser1.name = "Adv";
        regularUser1.password = "testUserPassword";
        regularUser1.permissions = GlobalPermission::accessAllMedia;

        regularUser2.email = "test2@test.com";
        regularUser2.fullName = "testUser2";
        regularUser2.isEnabled = true;
        regularUser2.name = "test_user_name2";
        regularUser2.password = "testUserPassword2";
        regularUser2.permissions = GlobalPermission::accessAllMedia;

        newAdminUser.email = "admin@test.com";
        newAdminUser.fullName = "testAdminUser";
        newAdminUser.isEnabled = true;
        newAdminUser.name = "test_admin_user_name";
        newAdminUser.password = "testAdminPassword";
        newAdminUser.permissions = GlobalPermission::admin;

        defaultAdmin.name = "admin";
        defaultAdmin.password = "admin";
    }
};

TEST_F(SaveUserEx, success)
{
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);

    whenSaveUserRequestIssued(defaultAdmin, newAdminUser, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(newAdminUser);
}

TEST_F(SaveUserEx, savedAdminLowerCaseMustBeAbleToLogin)
{
    whenSaveUserRequestIssued(defaultAdmin, newAdminUser, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(newAdminUser);
    thenSavedUserShouldBeAuthorizedByServer(newAdminUser);
}

TEST_F(SaveUserEx, savedUserUpperCaseMustBeAbleToLogin)
{
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);
    thenSavedUserShouldBeAuthorizedByServer(regularUser1);
}

TEST_F(SaveUserEx, nonAdminAccessShouldFail)
{
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);
    whenSaveUserRequestIssued(regularUser1, newAdminUser, http::StatusCode::forbidden);
}

TEST_F(SaveUserEx, shouldBeImpossibleToSaveNewUserWithSameName)
{
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::forbidden);
}

TEST_F(SaveUserEx, shouldBeImpossibleToChangeExistingUserNameIfAnotherUserHasIt)
{
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::ok);
    whenSaveUserRequestIssued(defaultAdmin, regularUser2, http::StatusCode::ok);

    thenUserShouldAppearInTheGetUsersResponse(regularUser1);
    thenUserShouldAppearInTheGetUsersResponse(regularUser2);

    whenIdFilled(&regularUser1);
    whenUserNameChanged(&regularUser1, regularUser2.name);
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::forbidden);
}

TEST_F(SaveUserEx, shouldBePossibleToSaveSameUserTwice)
{
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);

    whenIdFilled(&regularUser1);
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);
}

TEST_F(SaveUserEx, shouldBePossibleToChangeUserPassword)
{
    whenSaveUserRequestIssued(defaultAdmin, regularUser1, http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);

    whenIdFilled(&regularUser1);
    whenChangePasswordRequestIssued(regularUser1, &regularUser1, "new_password",
        http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);

    whenChangePasswordRequestIssued(regularUser1, &regularUser1, "new_password2",
        http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);

    whenChangePasswordRequestIssued(regularUser1, &regularUser1, "new_password",
        http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(regularUser1);
}

} // namespace nx::test
