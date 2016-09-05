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
        fps(0.0)
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
    (fps)

//-------------------------------------------------------------------------------------------------

struct ApiScheduleTaskWithRefData: ApiScheduleTaskData
{
    QnUuid sourceId;
};
#define ApiScheduleTaskWithRefData_Fields ApiScheduleTaskData_Fields (sourceId)

//-------------------------------------------------------------------------------------------------

struct ApiCameraAttributesData: ApiData
{
    ApiCameraAttributesData():
        scheduleEnabled(true),
        licenseUsed(true),
        motionType(Qn::MT_Default),
        audioEnabled(false),
        secondaryStreamQuality(Qn::SSQualityNotDefined),
        controlEnabled(true),
        minArchiveDays(0),
        maxArchiveDays(0),
        failoverPriority(Qn::FP_Medium),
        backupType(Qn::CameraBackup_Default)
    {
    }

    QnUuid cameraID; //< TODO: #mike cameraId
    QString cameraName;
    QString userDefinedGroupName;
    bool scheduleEnabled;
    bool licenseUsed;
    Qn::MotionType motionType;
    QnLatin1Array motionMask;
    std::vector<ApiScheduleTaskData> scheduleTasks;
    bool audioEnabled;
    Qn::SecondStreamQuality secondaryStreamQuality;
    bool controlEnabled;
    QnLatin1Array dewarpingParams;
    int minArchiveDays;
    int maxArchiveDays;
    QnUuid preferedServerId;
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
    (secondaryStreamQuality)\
    (controlEnabled) \
    (dewarpingParams) \
    (minArchiveDays) \
    (maxArchiveDays) \
    (preferedServerId) \
    (failoverPriority) \
    (backupType)
#define ApiCameraAttributesData_Fields (cameraID)(cameraName) ApiCameraAttributesData_Fields_Short

} // namespace ec2
