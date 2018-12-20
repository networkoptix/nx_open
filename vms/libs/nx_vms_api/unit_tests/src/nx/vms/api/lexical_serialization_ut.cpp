#include <gtest/gtest.h>

#include <nx/vms/api/types/resource_types.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/types/connection_types.h>

namespace nx::vms::api::test {

TEST(Lexical, enumSerialization)
{
    ASSERT_EQ(
        "Online",
        QnLexical::serialized(ResourceStatus::online).toStdString());
    ASSERT_EQ(
        "CSF_HasIssuesFlag",
        QnLexical::serialized<CameraStatusFlag>(CameraStatusFlag::CSF_HasIssuesFlag)
        .toStdString());
    ASSERT_EQ(
        "CSF_HasIssuesFlag",
        QnLexical::serialized<CameraStatusFlags>(CameraStatusFlag::CSF_HasIssuesFlag)
        .toStdString());
    ASSERT_EQ(
        "RT_MotionOnly",
        QnLexical::serialized<RecordingType>(RecordingType::motionOnly).toStdString());
    ASSERT_EQ(
        "normal",
        QnLexical::serialized<StreamQuality>(StreamQuality::normal).toStdString());
    ASSERT_EQ(
        "Low",
        QnLexical::serialized<FailoverPriority>(FailoverPriority::low).toStdString());
    ASSERT_EQ(
        "CameraBackupHighQuality",
        QnLexical::serialized<CameraBackupQuality>(CameraBackupQuality::CameraBackup_HighQuality)
        .toStdString());
    ASSERT_EQ(
        "CameraBackupHighQuality",
        QnLexical::serialized<CameraBackupQualities>(CameraBackupQuality::CameraBackup_HighQuality)
        .toStdString());
    ASSERT_EQ(
        "Form",
        QnLexical::serialized<IoModuleVisualStyle>(IoModuleVisualStyle::form).toStdString());
    ASSERT_EQ(
        "SF_RemoteEC",
        QnLexical::serialized<ServerFlag>(ServerFlag::SF_RemoteEC).toStdString());
    ASSERT_EQ(
        "SF_RemoteEC",
        QnLexical::serialized<ServerFlags>(ServerFlag::SF_RemoteEC).toStdString());
    ASSERT_EQ(
        "BackupManual",
        QnLexical::serialized<BackupType>(BackupType::manual).toStdString());

    ASSERT_EQ(
        "media",
        QnLexical::serialized<StreamDataFilters>(StreamDataFilter::media).toStdString());
    ASSERT_EQ(
        "media",
        QnLexical::serialized<StreamDataFilter>(StreamDataFilter::media).toStdString());
}

TEST(Lexical, simpleStringsEnumDeserialization)
{
    using Quality = CameraBackupQuality;
    using Qualities = CameraBackupQualities;

    ASSERT_EQ(
        QnLexical::deserialized<Quality>("CameraBackupHighQuality"),
        Quality::CameraBackup_HighQuality);

    ASSERT_EQ(
        QnLexical::deserialized<Qualities>("CameraBackupHighQuality"),
        Quality::CameraBackup_HighQuality);

    ASSERT_EQ(
        QnLexical::deserialized<Quality>("CameraBackupBoth"),
        Quality::CameraBackup_Both);
}

TEST(Lexical, combinedStringsEnumDeserialization)
{
    using Quality = CameraBackupQuality;
    using Qualities = CameraBackupQualities;

    ASSERT_EQ(
        QnLexical::deserialized<Qualities>("CameraBackupLowQuality|CameraBackupHighQuality"),
        Quality::CameraBackup_Both);
}

TEST(Lexical, numericDeserialization)
{
    using Quality = CameraBackupQuality;
    using Qualities = CameraBackupQualities;

    ASSERT_EQ(
        QnLexical::deserialized<Quality>("1"),
        Quality::CameraBackup_HighQuality);

    ASSERT_EQ(
        QnLexical::deserialized<Qualities>("1"),
        Quality::CameraBackup_HighQuality);

    ASSERT_EQ(
        QnLexical::deserialized<Quality>("3"),
        Quality::CameraBackup_Both);
}

TEST(Lexical, peerType)
{
    ASSERT_EQ("PT_OldSetver", QnLexical::serialized<PeerType>(PeerType::oldServer).toStdString());

    ASSERT_EQ(
        PeerType::oldServer,
        QnLexical::deserialized<PeerType>("PT_OldSetver"));
}

} // namespace nx::vms::api::test
