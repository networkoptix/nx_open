// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types/dewarping_types.h>

#include <nx/fusion/model_functions.h>
#include <nx/reflect/string_conversion.h>

namespace nx::vms::api::dewarping {

void PrintTo(const CameraScheduleTaskData& val, ::std::ostream* os)
{
    *os << QJson::serialized(val).toStdString();
}

namespace test {

namespace {

template<typename T>
QByteArray lexicalQuoted(T value)
{
    return QByteArray::fromStdString('"' + nx::reflect::enumeration::toString(value) + '"');
}

template<typename T>
QByteArray numericQuoted(T value)
{
    return QString("\"%1\"").arg((int) value).toUtf8();
}

} // namespace

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

TEST(Json, dewarpingEnumsLexicalSerialization)
{
    ASSERT_EQ(QJson::serialized(FisheyeCameraMount::wall),
        lexicalQuoted(FisheyeCameraMount::wall));

    ASSERT_EQ(QJson::serialized(CameraProjection::stereographic),
        lexicalQuoted(CameraProjection::stereographic));

    ASSERT_EQ(QJson::serialized(ViewProjection::equirectangular),
        lexicalQuoted(ViewProjection::equirectangular));
}

TEST(Json, dewarpingEnumsLexicalDeserialization)
{
    ASSERT_EQ(QJson::deserialized<FisheyeCameraMount>(lexicalQuoted(FisheyeCameraMount::table)),
        FisheyeCameraMount::table);

    ASSERT_EQ(QJson::deserialized<CameraProjection>(lexicalQuoted(
        CameraProjection::equirectangular360)), CameraProjection::equirectangular360);

    ASSERT_EQ(QJson::deserialized<ViewProjection>(lexicalQuoted(ViewProjection::rectilinear)),
        ViewProjection::rectilinear);
}

TEST(Json, dewarpingEnumsNumericDeserialization)
{
    ASSERT_EQ(QJson::deserialized<FisheyeCameraMount>(numericQuoted(FisheyeCameraMount::ceiling)),
        FisheyeCameraMount::ceiling);

    ASSERT_EQ(QJson::deserialized<CameraProjection>(numericQuoted(CameraProjection::equisolid)),
        CameraProjection::equisolid);

    ASSERT_EQ(QJson::deserialized<ViewProjection>(numericQuoted(ViewProjection::equirectangular)),
        ViewProjection::equirectangular);
}

} // namespace test
} // namespace nx::vms::api::dewarping
