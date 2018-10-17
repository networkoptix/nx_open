#include <memory>
#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <api/model/configure_system_data.h>
#include <nx/vms/api/data/user_data_ex.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/module_information.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"

namespace nx::test {

class SaveUserEx: public ::testing::Test
{
protected:
    using LauncherPtr = std::unique_ptr<MediaServerLauncher>;

    enum class ApiAccess
    {
        admin,
        nonAdmin
    };

    enum class UserCategory
    {
        admin,
        regular,
        regular2
    };

    SaveUserEx()
    {
        m_regularUser.email = "test@test.com";
        m_regularUser.fullName = "testUser";
        m_regularUser.isEnabled = true;
        m_regularUser.name = "test_user_name";
        m_regularUser.password = "testUserPassword";
        m_regularUser.permissions = GlobalPermission::accessAllMedia;

        m_regularUser2.email = "test2@test.com";
        m_regularUser2.fullName = "testUser2";
        m_regularUser2.isEnabled = true;
        m_regularUser2.name = "test_user_name2";
        m_regularUser2.password = "testUserPassword2";
        m_regularUser2.permissions = GlobalPermission::accessAllMedia;

        m_adminUser.email = "admin@test.com";
        m_adminUser.fullName = "testAdminUser";
        m_adminUser.isEnabled = true;
        m_adminUser.name = "test_admin_user_name";
        m_adminUser.password = "testAdminPassword";
        m_adminUser.permissions = GlobalPermission::admin;
    }

    LauncherPtr givenServer()
    {
        LauncherPtr result = std::unique_ptr<MediaServerLauncher>(new MediaServerLauncher());
        result->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        return result;
    }

    void whenServerLaunched(const LauncherPtr& server)
    {
        ASSERT_TRUE(server->start());
        vms::api::ModuleInformation moduleInformation;
        issueGetRequest(server.get(), "api/moduleInformation", moduleInformation);
        ASSERT_FALSE(moduleInformation.id.isNull());
    }

    void whenSaveUserRequestIssued(const LauncherPtr& server, ApiAccess apiAccess,
        UserCategory userCategory,
        network::http::StatusCode::Value expectedCode)
    {
        vms::api::UserDataEx* userDataToSave = selectData(userCategory);
        QString authName = apiAccess == ApiAccess::admin ? "admin" : m_regularUser.name;
        QString authPassword = apiAccess == ApiAccess::admin ? "admin" : m_regularUser.password;

        NX_TEST_API_POST(server.get(), "/ec2/saveUser", *userDataToSave, nullptr,
            expectedCode, authName, authPassword, &m_responseBuffer);

        NX_VERBOSE(this, lm("/ex2/saveUser issued successfully, response: %1").args(m_responseBuffer));
    }

    void thenUserShouldAppearInTheGetUsersResponse(const LauncherPtr& server,
        UserCategory userCategory)
    {
        NX_TEST_API_GET(server.get(), "/ec2/getUsers", &m_userDataList);
        QString userName;
        switch (userCategory)
        {
            case UserCategory::admin: userName = m_adminUser.name; break;
            case UserCategory::regular: userName = m_regularUser.name; break;
            case UserCategory::regular2: userName = m_regularUser2.name; break;
        }

        auto testUserIt = std::find_if(m_userDataList.cbegin(), m_userDataList.cend(),
            [&userName](const auto& userData) { return userData.name == userName; });

        ASSERT_NE(testUserIt, m_userDataList.cend());
    }

    void thenSavedUserShouldBeAuthorizedByServer(const LauncherPtr& server,
        UserCategory userCategory)
    {
        vms::api::UserDataList userDataList;

        QString authName = userCategory == UserCategory::admin
            ? m_adminUser.name : m_regularUser.name;
        QString authPassword = userCategory == UserCategory::admin
            ? m_adminUser.password : m_regularUser.password;

        NX_TEST_API_GET(server.get(), "/ec2/getUsers", &userDataList,
            network::http::StatusCode::ok, authName, authPassword);
    }

    void whenUserNameChanged(UserCategory toChange, UserCategory changeTo)
    {
        vms::api::UserDataEx* userDataToChange = selectData(toChange);
        vms::api::UserDataEx* userDataToChangeTo = selectData(changeTo);

        userDataToChange->name = userDataToChangeTo->name;
    }

    void whenIdFilled(UserCategory userCategory)
    {
        vms::api::UserDataEx* userDataToFillId = selectData(userCategory);
        for (const auto& existingUser: m_userDataList)
        {
            if (existingUser.name == userDataToFillId->name)
                userDataToFillId->id = existingUser.id;
        }
        ASSERT_FALSE(userDataToFillId->id.isNull());
    }

private:
    vms::api::UserDataEx m_adminUser;
    vms::api::UserDataEx m_regularUser;
    vms::api::UserDataEx m_regularUser2;
    vms::api::UserDataList m_userDataList;
    Buffer m_responseBuffer;

    template<typename ResponseData>
    void issueGetRequest(MediaServerLauncher* launcher, const QString& path,
        ResponseData& responseData)
    {
        QnJsonRestResult jsonRestResult;
        NX_TEST_API_GET(launcher, path, &jsonRestResult);
        responseData = jsonRestResult.deserialized<ResponseData>();
    }

    vms::api::UserDataEx* selectData(UserCategory category)
    {
        switch (category)
        {
            case UserCategory::admin: return &m_adminUser;
            case UserCategory::regular: return &m_regularUser;
            case UserCategory::regular2: return &m_regularUser2;
        }
        return nullptr;
    }
};

TEST_F(SaveUserEx, success)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular,
        /*expectedResult*/ network::http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular);

    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::admin,
        /*expectedResult*/ network::http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::admin);
}

TEST_F(SaveUserEx, savedUserMustBeAbleToLogin)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::admin,
        /*expectedResult*/ network::http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::admin);
    thenSavedUserShouldBeAuthorizedByServer(server, UserCategory::admin);
}

TEST_F(SaveUserEx, nonAdminAccessShouldFail)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular,
        /*expectedResult*/ network::http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular);

    whenSaveUserRequestIssued(server, ApiAccess::nonAdmin, UserCategory::admin,
        /*expectedResult*/ network::http::StatusCode::forbidden);
}

TEST_F(SaveUserEx, shouldBeImpossibleToSaveNewUserWithSameName)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular,
        /*expectedResult*/ network::http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular);

    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular,
        /*expectedResult*/ network::http::StatusCode::forbidden);
}

TEST_F(SaveUserEx, shouldBeImpossibleToChangeExistingUserNameIfAnotherUserHasIt)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular,
        /*expectedResult*/ network::http::StatusCode::ok);
    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular2,
        /*expectedResult*/ network::http::StatusCode::ok);

    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular2);

    whenIdFilled(UserCategory::regular);
    whenUserNameChanged(/*toChange*/ UserCategory::regular, /*changeTo*/ UserCategory::regular2);
    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular,
        network::http::StatusCode::forbidden);
}

TEST_F(SaveUserEx, shouldPossibleToSaveSameUserTwice)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular,
        /*expectedResult*/ network::http::StatusCode::ok);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular);

    whenIdFilled(UserCategory::regular);
    whenSaveUserRequestIssued(server, ApiAccess::admin, UserCategory::regular,
        network::http::StatusCode::ok);

    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular);
}

} // namespace nx::test
