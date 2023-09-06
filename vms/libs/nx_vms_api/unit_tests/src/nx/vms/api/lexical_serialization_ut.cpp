// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <nx/reflect/string_conversion.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/smtp_types.h>

namespace nx::vms::api {
namespace test {

using MotionStreamTypeData = std::pair<StreamIndex, std::vector<std::string>>;
class MotionStreamTypeLexicalTest: public ::testing::TestWithParam<MotionStreamTypeData>
{
};

TEST_P(MotionStreamTypeLexicalTest, assertParsedCorrectly)
{
    auto [input, output] = GetParam();
    const auto value = nx::reflect::toString<StreamIndex>(input);
    ASSERT_EQ(value, output[0]);

    for (const auto& sampleOutput: output)
    {
        auto parsed = StreamIndex::undefined;
        nx::reflect::fromString(sampleOutput, &parsed);
        ASSERT_EQ(input, parsed);
    }
}

// Vector defines list of values, which must be correctly deserialized.
// First value is the correct serialization value.
std::vector<MotionStreamTypeData> motionStreamTestData = {
    {StreamIndex::undefined, {"", "-1", "some_strange_string"}},
    {StreamIndex::primary, {"primary", "0"}},
    {StreamIndex::secondary, {"secondary", "1"}}
};

INSTANTIATE_TEST_SUITE_P(MotionStreamTypeLexicalSerialization,
    MotionStreamTypeLexicalTest,
    ::testing::ValuesIn(motionStreamTestData));

TEST(Lexical, enumSerialization)
{
    EXPECT_EQ("Online", nx::reflect::toString(ResourceStatus::online));
    EXPECT_EQ("CSF_HasIssuesFlag", nx::reflect::toString(CameraStatusFlag::CSF_HasIssuesFlag));
    EXPECT_EQ(
        "CSF_HasIssuesFlag",
        nx::reflect::toString(CameraStatusFlags(CameraStatusFlag::CSF_HasIssuesFlag)));
    EXPECT_EQ("metadataOnly", nx::reflect::toString(RecordingType::metadataOnly));
    EXPECT_EQ("normal", nx::reflect::toString(StreamQuality::normal));
    EXPECT_EQ("Low", nx::reflect::toString(FailoverPriority::low));
    EXPECT_EQ(
        "CameraBackupHighQuality",
        nx::reflect::toString(CameraBackupQuality::CameraBackupHighQuality));
    EXPECT_EQ(
        "CameraBackupHighQuality",
        nx::reflect::toString(
            CameraBackupQuality(CameraBackupQuality::CameraBackupHighQuality)));
    EXPECT_EQ("Form", nx::reflect::toString(IoModuleVisualStyle::form));
    EXPECT_EQ("SF_RemoteEC", nx::reflect::toString(ServerFlag::SF_RemoteEC));
    EXPECT_EQ("SF_RemoteEC", nx::reflect::toString(ServerFlags(ServerFlag::SF_RemoteEC)));

    EXPECT_EQ("media", nx::reflect::toString(StreamDataFilter::media));
    EXPECT_EQ("media", nx::reflect::toString(StreamDataFilters(StreamDataFilter::media)));
}

TEST(Lexical, deprecatedServerFlags)
{
    // The following Server Flags have been declared obsolete, but must be properly deserialized
    // from their original names for compatibility.

    EXPECT_EQ(ServerFlag::SF_deprecated_AutoSystemName,
        nx::reflect::fromString<ServerFlag>("SF_AutoSystemName"));

    EXPECT_EQ(ServerFlag::SF_deprecated_RequiresEdgeLicense,
        nx::reflect::fromString<ServerFlag>("SF_RequiresEdgeLicense"));

    EXPECT_EQ(ServerFlag::SF_deprecated_HasHDD,
        nx::reflect::fromString<ServerFlag>("SF_Has_HDD"));
}

TEST(Lexical, simpleStringsEnumDeserialization)
{
    using Quality = CameraBackupQuality;

    EXPECT_EQ(
        nx::reflect::fromString<Quality>("CameraBackupHighQuality"),
        Quality::CameraBackupHighQuality);


    EXPECT_EQ(
        nx::reflect::fromString<Quality>("CameraBackupBoth"),
        Quality::CameraBackupBoth);
}

TEST(Lexical, numericDeserialization)
{
    using Quality = CameraBackupQuality;

    EXPECT_EQ(nx::reflect::fromString<Quality>("1"), Quality::CameraBackupHighQuality);
    EXPECT_EQ(nx::reflect::fromString<Quality>("0"), Quality::CameraBackupBoth);
}

TEST(Lexical, peerType)
{
    EXPECT_EQ("PT_OldServer", nx::reflect::toString(PeerType::oldServer));
    EXPECT_EQ(PeerType::oldServer, nx::reflect::fromString<PeerType>("PT_OldServer"));
}

TEST(Lexical, smtpConnectionType)
{
    EXPECT_EQ("insecure", nx::reflect::toString(ConnectionType::insecure));
    EXPECT_EQ(ConnectionType::insecure, nx::reflect::fromString<ConnectionType>("insecure"));
}

// Compatibility with version < 5.0.
TEST(Lexical, recordingTypeCompatibility)
{
    EXPECT_EQ(RecordingType::always,
        nx::reflect::fromString<RecordingType>("RT_Always"));
    EXPECT_EQ(RecordingType::metadataOnly,
        nx::reflect::fromString<RecordingType>("RT_MotionOnly"));
    EXPECT_EQ(RecordingType::never,
        nx::reflect::fromString<RecordingType>("RT_Never"));
    EXPECT_EQ(RecordingType::metadataAndLowQuality,
        nx::reflect::fromString<RecordingType>("RT_MotionAndLowQuality"));
}

TEST(Lexical, recordingType)
{
    EXPECT_EQ(RecordingType::always, nx::reflect::fromString<RecordingType>("always"));
    EXPECT_EQ("always", nx::reflect::toString(RecordingType::always));
    EXPECT_EQ(RecordingType::metadataOnly, nx::reflect::fromString<RecordingType>("metadataOnly"));
    EXPECT_EQ("metadataOnly", nx::reflect::toString(RecordingType::metadataOnly));
    EXPECT_EQ(RecordingType::never, nx::reflect::fromString<RecordingType>("never"));
    EXPECT_EQ("never", nx::reflect::toString(RecordingType::never));
    EXPECT_EQ(RecordingType::metadataAndLowQuality,
        nx::reflect::fromString<RecordingType>("metadataAndLowQuality"));
    EXPECT_EQ("metadataAndLowQuality",
        nx::reflect::toString(RecordingType::metadataAndLowQuality));
}

TEST(Lexical, motionType)
{
    EXPECT_EQ(MotionType::default_, nx::reflect::fromString<MotionType>("default"));
    EXPECT_EQ("default", nx::reflect::toString(MotionType::default_));
    EXPECT_EQ(MotionType::hardware, nx::reflect::fromString<MotionType>("hardware"));
    EXPECT_EQ("hardware", nx::reflect::toString(MotionType::hardware));
    EXPECT_EQ(MotionType::software, nx::reflect::fromString<MotionType>("software"));
    EXPECT_EQ("software", nx::reflect::toString(MotionType::software));
    EXPECT_EQ(MotionType::window, nx::reflect::fromString<MotionType>("window"));
    EXPECT_EQ("window", nx::reflect::toString(MotionType::window));
    EXPECT_EQ(MotionType::none, nx::reflect::fromString<MotionType>("none"));
    EXPECT_EQ("none", nx::reflect::toString(MotionType::none));
}

} // namespace test
} // namespace nx::vms::api
