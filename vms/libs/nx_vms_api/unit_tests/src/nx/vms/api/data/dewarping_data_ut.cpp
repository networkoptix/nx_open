// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/reflect/json.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/types/dewarping_types.h>

namespace nx::vms::api::dewarping {

void PrintTo(const MediaData& val, ::std::ostream* os)
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

TEST(DewarpingData, enumsLexicalSerialization)
{
    ASSERT_EQ(QJson::serialized(FisheyeCameraMount::wall),
        lexicalQuoted(FisheyeCameraMount::wall));

    ASSERT_EQ(QJson::serialized(CameraProjection::stereographic),
        lexicalQuoted(CameraProjection::stereographic));

    ASSERT_EQ(QJson::serialized(ViewProjection::equirectangular),
        lexicalQuoted(ViewProjection::equirectangular));
}

TEST(DewarpingData, enumsLexicalDeserialization)
{
    ASSERT_EQ(QJson::deserialized<FisheyeCameraMount>(lexicalQuoted(FisheyeCameraMount::table)),
        FisheyeCameraMount::table);

    ASSERT_EQ(QJson::deserialized<CameraProjection>(lexicalQuoted(
        CameraProjection::equirectangular360)), CameraProjection::equirectangular360);

    ASSERT_EQ(QJson::deserialized<ViewProjection>(lexicalQuoted(ViewProjection::rectilinear)),
        ViewProjection::rectilinear);
}

TEST(DewarpingData, enumsNumericDeserialization)
{
    ASSERT_EQ(QJson::deserialized<FisheyeCameraMount>(numericQuoted(FisheyeCameraMount::ceiling)),
        FisheyeCameraMount::ceiling);

    ASSERT_EQ(QJson::deserialized<CameraProjection>(numericQuoted(CameraProjection::equisolid)),
        CameraProjection::equisolid);

    ASSERT_EQ(QJson::deserialized<ViewProjection>(numericQuoted(ViewProjection::equirectangular)),
        ViewProjection::equirectangular);
}

TEST(DewarpingData, fusionVsReflectCompatibility)
{
    MediaData sample{
        .enabled = true,
        .viewMode = FisheyeCameraMount::ceiling,
        .fovRot = 0.0000000000000000,
        .xCenter = 0.49947916666666664,
        .yCenter = 0.49907407407407406,
        .radius = 0.25000000000000000,
        .hStretch = 2.0,
        .cameraProjection = CameraProjection::equisolid,
        .sphereAlpha = 0.0,
        .sphereBeta = 0.0};

    QByteArray reflected = QByteArray::fromStdString(nx::reflect::json::serialize(sample));
    EXPECT_EQ(QJson::deserialized<MediaData>(reflected), sample);

    std::string fusioned = QJson::serialized(sample).toStdString();
    auto [data, result] = nx::reflect::json::deserialize<MediaData>(fusioned);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(data, sample);
}

} // namespace test
} // namespace nx::vms::api::dewarping
