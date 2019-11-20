#include "user_utils.h"

#include <api/test_api_requests.h>
#include <nx/utils/test_support/utils.h>

namespace nx::vms::server::test::test_support {

vms::api::UserDataEx createUser(
    MediaServerLauncher* server,
    nx::vms::api::GlobalPermission permissions,
    const QString& name,
    const QString& password)
{
    vms::api::UserDataEx userData;
    userData.permissions = permissions;
    userData.name = name;
    userData.password = password;

    using namespace nx::test;
    NX_GTEST_WRAP(NX_TEST_API_POST(server, "/ec2/saveUser", userData));

    return userData;
}

vms::api::UserDataEx getOwner(MediaServerLauncher* server)
{
    vms::api::UserDataList userDataList;
    using namespace nx::test;
    NX_GTEST_WRAP(NX_TEST_API_GET(server, "/ec2/getUsers", &userDataList));

    const auto ownerIt = std::find_if(
        userDataList.cbegin(), userDataList.cend(),
        [](const auto& userData) { return userData.name == "admin"; });

    NX_GTEST_ASSERT_NE(userDataList.cend(), ownerIt);
    vms::api::UserDataEx result;
    static_cast<vms::api::UserData&>(result) = *ownerIt;
    result.password = "admin";

    return result;
}

} // namespace nx::vms::server::test::test_support
