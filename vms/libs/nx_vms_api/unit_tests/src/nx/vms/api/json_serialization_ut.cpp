#include <gtest/gtest.h>

#include <nx/vms/api/data/camera_attributes_data.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

void PrintTo(const ScheduleTaskData& val, ::std::ostream* os)
{
    *os << QJson::serialized(val).toStdString();
}

} // namespace nx::vms::api

namespace nx::vms::api::test {

TEST(Json, structSerialization)
{
    const std::string kSerializedStruct =
        R"({"bitrateKbps":0,"dayOfWeek":1,"endTime":0,"fps":0,"recordingType":"RT_Always","startTime":0,"streamQuality":"undefined"})";

    const ScheduleTaskData data;
    ASSERT_EQ(kSerializedStruct, QJson::serialized(data).toStdString());
}

TEST(Json, structDeserializationLexical)
{
    const auto kSerializedStruct =
        R"({"bitrateKbps":0,"dayOfWeek":1,"endTime":0,"fps":0,"recordingType":"RT_Always","startTime":0,"streamQuality":"undefined"})";

    const ScheduleTaskData data;
    ASSERT_EQ(data, QJson::deserialized<ScheduleTaskData>(kSerializedStruct));
}

TEST(Json, structDeserializationDefaults)
{
    const auto kSerializedStructEmpty = R"({})";
    const auto kSerializedStructQualityEmpty = R"({"streamQuality":""})";

    const ScheduleTaskData data;
    ASSERT_EQ(data, QJson::deserialized<ScheduleTaskData>(kSerializedStructEmpty));
    ASSERT_EQ(data, QJson::deserialized<ScheduleTaskData>(kSerializedStructQualityEmpty));
}

TEST(Json, structDeserializationNumeric)
{
    const auto kSerializedStructWithStrings = R"({"recordingType":"3","streamQuality":"5"})";
    const auto kSerializedStructWithNumbers = R"({"recordingType":3,"streamQuality":5})";

    ScheduleTaskData data;
    data.recordingType = RecordingType::motionAndLow;
    data.streamQuality = StreamQuality::preset;

    ASSERT_EQ(data, QJson::deserialized<ScheduleTaskData>(kSerializedStructWithStrings));
    ASSERT_EQ(data, QJson::deserialized<ScheduleTaskData>(kSerializedStructWithNumbers));
}

} // namespace nx::vms::api::test
