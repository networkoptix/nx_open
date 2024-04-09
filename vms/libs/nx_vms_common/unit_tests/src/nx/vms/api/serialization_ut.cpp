// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <api/model/api_ioport_data.h>
#include <api/model/audit/audit_record.h>
#include <api/model/configure_system_data.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/rest/json_reflect_result.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/api/types/device_type.h>

namespace nx::vms::api::test {

TEST(ConfigureSystemData, deserialization)
{
    ConfigureSystemData data1;

    data1.localSystemId = nx::Uuid::createUuid();
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
    data1.rewriteLocalSettings = true;
    data1.systemName = "system1";
    data1.currentPassword = "123";

    auto serializedData = QJson::serialized(data1);
    auto data2 = QJson::deserialized<ConfigureSystemData>(serializedData);

    ASSERT_EQ(data1, data2);
}

TEST(DeviceType, serialization)
{
    using namespace nx::vms::api;

    ASSERT_EQ("Camera", nx::reflect::toString(DeviceType::camera));
    ASSERT_EQ(DeviceType::camera, nx::reflect::fromString<DeviceType>("Camera"));
    ASSERT_EQ(DeviceType::camera, nx::reflect::fromString<DeviceType>("1"));
}

TEST(IOPortType, serialization)
{
    QByteArray result;
    Qn::IOPortType data = Qn::PT_Input;
    QJson::serialize(data, &result);
    ASSERT_EQ(result, "\"Input\"");
}

TEST(IOPortTypes, serialization)
{
    QByteArray result;
    Qn::IOPortTypes data = Qn::PT_Input | Qn::PT_Output;
    QJson::serialize(data, &result);
    ASSERT_EQ(result, "\"Input|Output\"");
}

TEST(QnAuditRecord, serialization)
{
    QnAuditRecord record{{{nx::Uuid{}}}};
    record.details = ResourceDetails{{{nx::Uuid()}}, {"detailed description"}};
    nx::network::rest::JsonReflectResult<QnAuditRecordList> result;
    result.reply.push_back(record);
    const std::string expected = /*suppress newline*/ 1 + R"json(
{
    "error": "0",
    "errorString": "",
    "reply": [
        {
            "eventType": "notDefined",
            "createdTimeS": "0",
            "authSession": {
                "id": "{00000000-0000-0000-0000-000000000000}",
                "userName": "",
                "userHost": "",
                "userAgent": ""
            },
            "details": {
                "ids": [
                    "{00000000-0000-0000-0000-000000000000}"
                ],
                "description": "detailed description"
            }
        }
    ]
})json";
    ASSERT_EQ(
        expected, nx::utils::formatJsonString(nx::reflect::json::serialize(result)).toStdString());
}

} // namespace nx::vms::api::test
