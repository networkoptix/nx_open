#include <gtest/gtest.h>

#include <nx/vms/api/data/resource_data.h>
#include <common/common_globals.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical_enum.h>

namespace nx {
namespace vms {
namespace api {
namespace test {

TEST(StatusData, serialization)
{
    ASSERT_EQ(
        "Online",
        QnLexical::serialized(ResourceStatus::online).toStdString());
    ASSERT_EQ(
        "CSF_HasIssuesFlag",
        QnLexical::serialized<CameraStatusFlag>(CameraStatusFlag::CSF_HasIssuesFlag).toStdString());
    ASSERT_EQ(
        "CSF_HasIssuesFlag",
        QnLexical::serialized<CameraStatusFlags>(CameraStatusFlag::CSF_HasIssuesFlag).toStdString());
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
        QnLexical::serialized<CameraBackupQuality>(CameraBackupQuality::CameraBackup_HighQuality).toStdString());
    ASSERT_EQ(
        "CameraBackupHighQuality",
        QnLexical::serialized<CameraBackupQualities>(CameraBackupQuality::CameraBackup_HighQuality).toStdString());
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
}

}
}
}
}
