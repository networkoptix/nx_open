// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/rest/json_reflect_result.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/api/data/audit.h>
#include <nx/vms/api/data/camera_attributes_data.h>

namespace nx::vms::api::dewarping {

void PrintTo(const CameraScheduleTaskData& val, ::std::ostream* os)
{
    *os << QJson::serialized(val).toStdString();
}

namespace test {


TEST(Json, structSerialization)
{
    const std::string kSerializedStruct =
        R"({"bitrateKbps":0,"dayOfWeek":1,"endTime":0,"fps":0,"metadataTypes":"none","recordingType":"always","startTime":0,"streamQuality":"undefined"})";

    const CameraScheduleTaskData data;
    ASSERT_EQ(kSerializedStruct, QJson::serialized(data).toStdString());
}

TEST(Json, structDeserializationLexical)
{
    const auto kSerializedStruct = R"(
    {
        "bitrateKbps": 0,
        "dayOfWeek": 1,
        "endTime": 0,
        "fps": 0,
        "metadataTypes": "none",
        "recordingType": "RT_Always",
        "startTime": 0,
        "streamQuality": "undefined"
    })";

    const CameraScheduleTaskData data;
    ASSERT_EQ(data, QJson::deserialized<CameraScheduleTaskData>(kSerializedStruct));
}

TEST(Json, structDeserializationDefaults)
{
    const auto kSerializedStructEmpty = R"({})";
    const auto kSerializedStructQualityEmpty = R"({"streamQuality":""})";

    const CameraScheduleTaskData data;
    ASSERT_EQ(data, QJson::deserialized<CameraScheduleTaskData>(kSerializedStructEmpty));
    ASSERT_EQ(data, QJson::deserialized<CameraScheduleTaskData>(kSerializedStructQualityEmpty));
}

TEST(Json, structDeserializationNumeric)
{
    const auto kSerializedStructWithStrings = R"({"recordingType":"3","streamQuality":"5"})";
    const auto kSerializedStructWithNumbers = R"({"recordingType":3,"streamQuality":5})";

    CameraScheduleTaskData data;
    data.recordingType = RecordingType::metadataAndLowQuality;
    data.streamQuality = StreamQuality::preset;

    ASSERT_EQ(data, QJson::deserialized<CameraScheduleTaskData>(kSerializedStructWithStrings));
    ASSERT_EQ(data, QJson::deserialized<CameraScheduleTaskData>(kSerializedStructWithNumbers));
}

TEST(Json, AuditRecord)
{
    AuditRecord record{{{nx::Uuid{}}}};
    record.details = ResourceDetails{{{nx::Uuid()}}, {"detailed description"}};
    nx::network::rest::JsonReflectResult<AuditRecordList> result;
    result.reply.push_back(record);
    const std::string expected = /*suppress newline*/ 1 + R"json(
{
    "error": "0",
    "errorString": "",
    "reply": [
        {
            "serverId": "{00000000-0000-0000-0000-000000000000}",
            "eventType": "notDefined",
            "createdTimeS": 0,
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

} // namespace test
} // namespace nx::vms::api::dewarping
