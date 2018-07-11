#pragma once

#include "data.h"

#include <vector>

#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/utils/latin1_array.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API ScheduleTaskData: Data
{
    qint32 startTime = 0;
    qint32 endTime = 0;
    RecordingType recordingType = RecordingType::always;
    qint8 dayOfWeek = 1;
    StreamQuality streamQuality = StreamQuality::undefined;
    qint16 fps = 0;
    int bitrateKbps = 0;
};
#define ScheduleTaskData_Fields \
    (startTime) \
    (endTime) \
    (recordingType) \
    (dayOfWeek) \
    (streamQuality) \
    (fps) \
    (bitrateKbps)

//-------------------------------------------------------------------------------------------------

// TODO: #GDM Move this class to DbManager.
struct NX_VMS_API ScheduleTaskWithRefData: ScheduleTaskData
{
    QnUuid sourceId;
};
#define ScheduleTaskWithRefData_Fields ScheduleTaskData_Fields (sourceId)

//-------------------------------------------------------------------------------------------------

static constexpr int kDefaultMinArchiveDays = 1;
static constexpr int kDefaultMaxArchiveDays = 30;
static constexpr int kDefaultRecordBeforeMotionSec = 5;
static constexpr int kDefaultRecordAfterMotionSec = 5;

struct NX_VMS_API CameraAttributesData: Data
{
    QnUuid getIdForMerging() const { return cameraId; } //< See IdData::getIdForMerging().

    static DeprecatedFieldNames* getDeprecatedFieldNames();

    QnUuid cameraId;
    QString cameraName;
    QString userDefinedGroupName;
    bool scheduleEnabled = true;
    bool licenseUsed = true;       //< TODO: #GDM Field is not used.
    MotionType motionType = MT_Default;
    QnLatin1Array motionMask;
    std::vector<ScheduleTaskData> scheduleTasks;
    bool audioEnabled = false;
    bool disableDualStreaming = false; //< TODO: #GDM Double negation.
    bool controlEnabled = true;
    QnLatin1Array dewarpingParams;
    int minArchiveDays = -kDefaultMinArchiveDays; //< Negative means 'auto'.
    int maxArchiveDays = -kDefaultMaxArchiveDays; //< Negative means 'auto'.
    QnUuid preferredServerId;
    FailoverPriority failoverPriority = FailoverPriority::medium;
    CameraBackupQualities backupType = CameraBackup_Default;
    QString logicalId;
    int recordBeforeMotionSec = kDefaultRecordBeforeMotionSec;
    int recordAfterMotionSec = kDefaultRecordAfterMotionSec;
};
#define CameraAttributesData_Fields_Short \
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
    (logicalId) \
    (recordBeforeMotionSec) \
    (recordAfterMotionSec) \

#define CameraAttributesData_Fields (cameraId)(cameraName) CameraAttributesData_Fields_Short

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::ScheduleTaskData)
Q_DECLARE_METATYPE(nx::vms::api::ScheduleTaskWithRefData)
Q_DECLARE_METATYPE(nx::vms::api::CameraAttributesData)
Q_DECLARE_METATYPE(nx::vms::api::CameraAttributesDataList)
