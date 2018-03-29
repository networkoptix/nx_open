#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

struct ApiScheduleTaskData: ApiData
{
    qint32 startTime = 0;
    qint32 endTime = 0;
    Qn::RecordingType recordingType = Qn::RT_Always;
    qint8 dayOfWeek = 1;
    qint16 beforeThreshold = 0;
    qint16 afterThreshold = 0;
    Qn::StreamQuality streamQuality = Qn::QualityNotDefined;
    qint16 fps = 0;
    int bitrateKbps = 0;
};
#define ApiScheduleTaskData_Fields \
    (startTime) \
    (endTime) \
    (recordingType) \
    (dayOfWeek) \
    (beforeThreshold) \
    (afterThreshold) \
    (streamQuality) \
    (fps) \
    (bitrateKbps)

//-------------------------------------------------------------------------------------------------

struct ApiScheduleTaskWithRefData: ApiScheduleTaskData
{
    QnUuid sourceId;
};
#define ApiScheduleTaskWithRefData_Fields ApiScheduleTaskData_Fields (sourceId)

//-------------------------------------------------------------------------------------------------

static constexpr int kDefaultMinArchiveDays = 1;
static constexpr int kDefaultMaxArchiveDays = 30;

struct ApiCameraAttributesData: ApiData
{
    QnUuid getIdForMerging() const { return cameraId; } //< See ApiIdData::getIdForMerging().

    static DeprecatedFieldNames* getDeprecatedFieldNames()
    {
        static DeprecatedFieldNames kDeprecatedFieldNames{
            {lit("cameraId"), lit("cameraID")}, //< up to v2.6
            {lit("preferredServerId"), lit("preferedServerId")}, //< up to v2.6
        };
        return &kDeprecatedFieldNames;
    }

    QnUuid cameraId;
    QString cameraName;
    QString userDefinedGroupName;
    bool scheduleEnabled = true;
    bool licenseUsed = true;       //< TODO: #GDM Field is not used.
    Qn::MotionType motionType = Qn::MT_Default;
    QnLatin1Array motionMask;
    std::vector<ApiScheduleTaskData> scheduleTasks;
    bool audioEnabled = false;
    bool disableDualStreaming = false; //< TODO: #GDM Double negation.
    bool controlEnabled = true;
    QnLatin1Array dewarpingParams;
    int minArchiveDays = -kDefaultMinArchiveDays; //< Negative means 'auto'.
    int maxArchiveDays = -kDefaultMaxArchiveDays; //< Negative means 'auto'.
    QnUuid preferredServerId;
    Qn::FailoverPriority failoverPriority = Qn::FP_Medium;
    Qn::CameraBackupQualities backupType = Qn::CameraBackup_Default;
    QString logicalId;
};
#define ApiCameraAttributesData_Fields_Short \
    (userDefinedGroupName) \
    (scheduleEnabled) \
    (licenseUsed) \
    (motionType) \
    (motionMask) \
    (scheduleTasks) \
    (audioEnabled) \
    (disableDualStreaming)\
    (controlEnabled) \
    (dewarpingParams) \
    (minArchiveDays) \
    (maxArchiveDays) \
    (preferredServerId) \
    (failoverPriority) \
    (backupType) \
    (logicalId)
#define ApiCameraAttributesData_Fields (cameraId)(cameraName) ApiCameraAttributesData_Fields_Short

} // namespace ec2
