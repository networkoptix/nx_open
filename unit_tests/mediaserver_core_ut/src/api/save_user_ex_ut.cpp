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
        regular
    };

    SaveUserEx()
    {
        m_nonAdminUser.email = "test@test.com";
        m_nonAdminUser.fullName = "testUser";
        m_nonAdminUser.isCloud = false;
        m_nonAdminUser.isEnabled = true;
        m_nonAdminUser.isLdap = false;
        m_nonAdminUser.name = "test_user_name";
        m_nonAdminUser.password = "testUserPassword";
        m_nonAdminUser.permissions = GlobalPermission::accessAllMedia;

        m_adminUser.email = "admin@test.com";
        m_adminUser.fullName = "testAdminUser";
        m_adminUser.isCloud = false;
        m_adminUser.isEnabled = true;
        m_adminUser.isLdap = false;
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

    void whenSaveUserExRequestIssued(const LauncherPtr& server, ApiAccess apiAccess,
        UserCategory userCategory)
    {
        vms::api::UserDataEx* userDataToSave = userCategory == UserCategory::regular
            ? &m_nonAdminUser : &m_adminUser;

        QString authName = apiAccess == ApiAccess::admin ? "admin" : m_nonAdminUser.name;
        QString authPassword = apiAccess == ApiAccess::admin ? "admin" : m_nonAdminUser.password;

        NX_TEST_API_POST(server.get(), "/ec2/saveUserEx", *userDataToSave, nullptr,
            network::http::StatusCode::ok, authName, authPassword, &m_responseBuffer);
    }

    void thenUserShouldAppearInTheGetUsersResponse(const LauncherPtr& server,
        UserCategory userCategory)
    {
        vms::api::UserDataList userDataList;
        NX_TEST_API_GET(server.get(), "/ec2/getUsers", &userDataList);

        QString userName;
        if (userCategory == UserCategory::regular)
            userName = m_nonAdminUser.name;
        else
            userName = m_adminUser.name;

        auto testUserIt = std::find_if(userDataList.cbegin(), userDataList.cend(),
            [&userName](const auto& userData) { return userData.name == userName; });

        ASSERT_NE(testUserIt, userDataList.cend());
    }

    void thenSavedUserShouldBeAuthorizedByServer(const LauncherPtr& server,
        UserCategory userCategory)
    {
        vms::api::UserDataList userDataList;

        QString authName = userCategory == UserCategory::admin ? m_adminUser.name : m_nonAdminUser.name;
        QString authPassword = userCategory == UserCategory::admin ? m_adminUser.password : m_nonAdminUser.password;

        NX_TEST_API_GET(server.get(), "/ec2/getUsers", &userDataList, network::http::StatusCode::ok,
            authName, authPassword);
    }

    void thenResponseShouldContainForbiddenError()
    {
        QnJsonRestResult jsonRestResult;
        ASSERT_TRUE(QJson::deserialize(m_responseBuffer, &jsonRestResult));
        ASSERT_EQ(jsonRestResult.error, QnJsonRestResult::Error::Forbidden);
    }

private:
    vms::api::UserDataEx m_adminUser;
    vms::api::UserDataEx m_nonAdminUser;
    Buffer m_responseBuffer;

    template<typename ResponseData>
    void issueGetRequest(MediaServerLauncher* launcher, const QString& path,
        ResponseData& responseData)
    {
        QnJsonRestResult jsonRestResult;
        NX_TEST_API_GET(launcher, path, &jsonRestResult);
        responseData = jsonRestResult.deserialized<ResponseData>();
    }
};

TEST_F(SaveUserEx, success)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserExRequestIssued(server, ApiAccess::admin, UserCategory::regular);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular);

    whenSaveUserExRequestIssued(server, ApiAccess::admin, UserCategory::admin);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::admin);
}

TEST_F(SaveUserEx, savedUserMustBeAbleToLogin)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserExRequestIssued(server, ApiAccess::admin, UserCategory::admin);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::admin);
    thenSavedUserShouldBeAuthorizedByServer(server, UserCategory::admin);
}

TEST_F(SaveUserEx, nonAdminAccessShouldFail)
{
    auto server = givenServer();
    whenServerLaunched(server);

    whenSaveUserExRequestIssued(server, ApiAccess::admin, UserCategory::regular);
    thenUserShouldAppearInTheGetUsersResponse(server, UserCategory::regular);

    whenSaveUserExRequestIssued(server, ApiAccess::nonAdmin, UserCategory::admin);
    thenResponseShouldContainForbiddenError();
}

} // namespace nx::test
