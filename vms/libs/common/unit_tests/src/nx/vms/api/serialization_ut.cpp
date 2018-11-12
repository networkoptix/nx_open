#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <api/model/configure_system_data.h>

namespace nx::vms::api::test {

TEST(ConfigureSystemData, deserialization)
{
    ConfigureSystemData data1;

    data1.localSystemId = QnUuid::createUuid();
    data1.wholeSystem = true;
    data1.sysIdTime = 12345;
    data1.tranLogTime = nx::vms::api::Timestamp(1, 2);
    data1.port = 7070;
    data1.foreignServer.name = "server1";
    UserData user1;
    user1.name = "user1";
    data1.foreignUsers.push_back(user1);
    nx::vms::api::ResourceParamData setting1("key1", "value1");
    data1.foreignSettings.push_back(setting1);
    nx::vms::api::ResourceParamWithRefData param1(
        QnUuid::createUuid(),
        "paramName1", "paramValue1");
    data1.additionParams.push_back(param1);
    data1.rewriteLocalSettings = true;
    data1.systemName = "system1";
    data1.currentPassword = "123";

    auto serializedData = QJson::serialized(data1);
    auto data2 = QJson::deserialized<ConfigureSystemData>(serializedData);

    ASSERT_EQ(data1, data2);

}
} // namespace nx::vms::api::test
