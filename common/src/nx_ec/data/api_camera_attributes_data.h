#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

struct ApiScheduleTaskData: ApiData
{
    ApiScheduleTaskData():
        startTime(0),
        endTime(0),
        recordAudio(false),
        recordingType(Qn::RT_Always),
        dayOfWeek(1),
        beforeThreshold(0),
        afterThreshold(0),
        streamQuality(Qn::QualityNotDefined),
        fps(0.0),
        bitrateKbps(0)
    {
    }

    qint32 startTime;
    qint32 endTime;
    bool recordAudio;
    Qn::RecordingType recordingType;
    qint8 dayOfWeek;
    qint16 beforeThreshold;
    qint16 afterThreshold;
    Qn::StreamQuality streamQuality;
    qint16 fps;
    int bitrateKbps = 0;
};
#define ApiScheduleTaskData_Fields \
    (startTime) \
    (endTime) \
    (recordAudio) \
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

const int kDefaultMinArchiveDays = 1;
const int kDefaultMaxArchiveDays = 30;

struct ApiCameraAttributesData: ApiData
{
    ApiCameraAttributesData():
        scheduleEnabled(true),
        licenseUsed(true),
        motionType(Qn::MT_Default),
        audioEnabled(false),
        disableDualStreaming(false),
        controlEnabled(true),
        minArchiveDays(-kDefaultMinArchiveDays),
        maxArchiveDays(-kDefaultMaxArchiveDays),
        failoverPriority(Qn::FP_Medium),
        backupType(Qn::CameraBackup_Default)
    {
    }

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
    bool scheduleEnabled;
    bool licenseUsed;
    Qn::MotionType motionType;
    QnLatin1Array motionMask;
    std::vector<ApiScheduleTaskData> scheduleTasks;
    bool audioEnabled;
    bool disableDualStreaming;
    bool controlEnabled;
    QnLatin1Array dewarpingParams;
    int minArchiveDays;
    int maxArchiveDays;
    QnUuid preferredServerId;
    Qn::FailoverPriority failoverPriority;
    Qn::CameraBackupQualities backupType;
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
    (backupType)
#define ApiCameraAttributesData_Fields (cameraId)(cameraName) ApiCameraAttributesData_Fields_Short

} // namespace ec2
