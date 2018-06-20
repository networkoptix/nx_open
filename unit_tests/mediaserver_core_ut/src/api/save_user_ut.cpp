#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx_ec/data/api_user_data.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

namespace {

/**
 * Assert that the list contains a user with the specified email, and retrieve this user.
 */
#define NX_FIND_USER_BY_EMAIL(...) ASSERT_NO_FATAL_FAILURE(findUserByEmail(__VA_ARGS__))
static void findUserByEmail(
    const ec2::ApiUserDataList& users,
    ec2::ApiUserData* outUser,
    const QString email)
{
    ASSERT_TRUE(users.size() > 0);

    const auto foundUser = std::find_if(users.cbegin(), users.cend(),
        [email](const ec2::ApiUserData& u) { return u.email == email; });

    ASSERT_TRUE(foundUser != users.cend());

    *outUser = *foundUser; //< Copy struct by value.
}

} // namespace

TEST(SaveUser, fillDigestAndName)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    ec2::ApiUserDataList users;
    ec2::ApiUserData receivedUserData;
    ec2::ApiUserData userData;
    userData.fullName = "Tester Fullname";
    userData.permissions = GlobalPermission::admin;
    userData.email = "cloud-tester@email.com";
    userData.isCloud = true;

    NX_LOG("[TEST] Create a Cloud user, not specifying digest.", cl_logINFO);
    NX_TEST_API_POST(&launcher, "/ec2/saveUser", userData,
        keepOnlyJsonFields({"fullName", "permissions", "email", "isCloud"}));

    NX_LOG("[TEST] Retrieve the created user.", cl_logINFO);
    NX_TEST_API_GET(&launcher, lit("/ec2/getUsers"), &users);
    NX_FIND_USER_BY_EMAIL(users, &receivedUserData, userData.email);

    ASSERT_FALSE(receivedUserData.id.isNull());
    ASSERT_TRUE(receivedUserData.isCloud);

    // Digest and name should be properly filled by the server.
    ASSERT_EQ(userData.email, receivedUserData.email);
    ASSERT_EQ(receivedUserData.email, receivedUserData.name);
    ASSERT_EQ(ec2::ApiUserData::kCloudPasswordStub, receivedUserData.digest);
}

} // namespace test
} // namespace nx
