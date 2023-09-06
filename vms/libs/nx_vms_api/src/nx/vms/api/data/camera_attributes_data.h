// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/utils/latin1_array.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/resource_types.h>

#include "check_resource_exists.h"
#include "data_macros.h"
#include "schedule_task_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API CameraScheduleTaskData: public ScheduleTaskData
{
    RecordingType recordingType = RecordingType::always;

    /**%apidoc[opt] Quality of the recording. */
    StreamQuality streamQuality = StreamQuality::undefined;

    /**%apidoc[opt] Frames per second. */
    int fps = 0;

    /**%apidoc[opt] Bitrate. */
    int bitrateKbps = 0;

    /**%apidoc[opt] Types of metadata, which should trigger recording in case of corresponding
     * `recordingType` values.
     */
    RecordingMetadataTypes metadataTypes;

    bool operator==(const CameraScheduleTaskData& other) const = default;
};
#define CameraScheduleTaskData_Fields \
    ScheduleTaskData_Fields \
    (recordingType) \
    (streamQuality) \
    (fps) \
    (bitrateKbps) \
    (metadataTypes)

NX_REFLECTION_INSTRUMENT(CameraScheduleTaskData, CameraScheduleTaskData_Fields)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(CameraScheduleTaskData)

//-------------------------------------------------------------------------------------------------

static constexpr std::chrono::days kDefaultMinArchivePeriod(1);
static constexpr std::chrono::days kDefaultMaxArchivePeriod(30);
static constexpr int kDefaultRecordBeforeMotionSec = 5;
static constexpr int kDefaultRecordAfterMotionSec = 5;

/**%apidoc Defines content to backup. It is the set of flags. */
NX_REFLECTION_ENUM_CLASS(BackupContentType,
    archive = 1 << 0,
    motion = 1 << 1,
    analytics = 1 << 2,
    bookmarks = 1 << 3
)

NX_REFLECTION_ENUM_CLASS(BackupPolicy,
    byDefault = -1,
    off = 0,
    on = 1
)

Q_DECLARE_FLAGS(BackupContentTypes, BackupContentType)
Q_DECLARE_OPERATORS_FOR_FLAGS(BackupContentTypes)

/**%apidoc Additional device attributes. */
struct NX_VMS_API CameraAttributesData
{
    QnUuid getIdForMerging() const { return cameraId; } //< See IdData::getIdForMerging().
    QnUuid getId() const { return cameraId; }
    bool operator==(const CameraAttributesData& other) const = default;

    static DeprecatedFieldNames* getDeprecatedFieldNames();

    QnUuid cameraId;

    /**%apidoc[opt] Device name. */
    QString cameraName;

    /**%apidoc[opt] Name of the user-defined device group. */
    QString userDefinedGroupName;

    /**%apidoc[opt] Whether recording to the archive is enabled for the device. */
    bool scheduleEnabled = false;

    /**%apidoc[opt] Type of motion detection method. */
    MotionType motionType = MotionType::default_;

    /**%apidoc[opt] List of motion detection areas and their sensitivity. The format is
     * proprietary and is likely to change in future API versions. Currently, this string defines
     * several rectangles separated with ":", each rectangle is described by 5 comma-separated
     * numbers: sensitivity, x and y (for left top corner), width, height.
     */
    QnLatin1Array motionMask;

    /**%apidoc[opt] List of scheduleTask objects which define the device recording schedule. */
    std::vector<CameraScheduleTaskData> scheduleTasks;

    /**%apidoc[opt] Whether audio is enabled on the device. */
    bool audioEnabled = false;

    /**%apidoc[opt] Whether dual streaming is enabled if it is supported by device. */
    bool disableDualStreaming = false; //< TODO: #sivanov Double negation.

    /**%apidoc[opt] Whether server manages the device (changes resolution, FPS, create profiles,
     * etc).
     */
    bool controlEnabled = true;

    /**%apidoc[opt] Image dewarping parameters. The format is proprietary and is likely to change
     * in future API versions.
     */
    QnLatin1Array dewarpingParams;

    /**%apidoc[opt] Minimum number of seconds to keep the archive for. If the value is less than or
     * equal to zero, it is not used.
     */
    std::chrono::seconds minArchivePeriodS = -kDefaultMinArchivePeriod; //< Negative means 'auto'.

    /**%apidoc[opt] Maximum number of seconds to keep the archive for. If the value is less than or
     * equal zero, it is not used.
     */
    std::chrono::seconds maxArchivePeriodS = -kDefaultMaxArchivePeriod; //< Negative means 'auto'.

    /**%apidoc[opt] Unique id of a server which has the highest priority of hosting the device for
     * failover (if the current server fails).
     */
    QnUuid preferredServerId;

    /**%apidoc[opt] Priority for the device to be moved to another server for failover (if the
     * current server fails).
     */
    FailoverPriority failoverPriority = FailoverPriority::medium;

    /**%apidoc[opt] Combination (via "|") of the flags defining backup options. */
    CameraBackupQuality backupQuality = CameraBackupDefault;

    /**%apidoc[opt] Logical id of the device, set by user. */
    QString logicalId;

    /**%apidoc[opt] The number of seconds before a motion event to record the video for. */
    int recordBeforeMotionSec = kDefaultRecordBeforeMotionSec;
    /**%apidoc[opt] The number of seconds after a motion event to record the video for. */
    int recordAfterMotionSec = kDefaultRecordAfterMotionSec;
    BackupContentTypes backupContentType = BackupContentType::archive; //< What to backup content wise.
    BackupPolicy backupPolicy = BackupPolicy::byDefault;

    /** Used by ...Model::toDbTypes() and transaction-description-modify checkers. */
    CheckResourceExists checkResourceExists = CheckResourceExists::yes; /**<%apidoc[unused] */
};
#define CameraAttributesData_Fields_Short \
    (userDefinedGroupName) \
    (scheduleEnabled) \
    (motionType) \
    (motionMask) \
    (scheduleTasks) \
    (audioEnabled) \
    (disableDualStreaming)\
    (controlEnabled) \
    (dewarpingParams) \
    (minArchivePeriodS) \
    (maxArchivePeriodS) \
    (preferredServerId) \
    (failoverPriority) \
    (backupQuality) \
    (logicalId) \
    (recordBeforeMotionSec) \
    (recordAfterMotionSec) \
    (backupContentType) \
    (backupPolicy)

#define CameraAttributesData_Fields (cameraId)(cameraName) CameraAttributesData_Fields_Short

NX_VMS_API_DECLARE_STRUCT_AND_LIST(CameraAttributesData)

NX_VMS_API void serialize(
    QnJsonContext* ctx, const CameraAttributesData& value, QJsonValue* target);
NX_VMS_API bool deserialize(
    QnJsonContext* ctx, const QJsonValue& value, CameraAttributesData* target);

} // namespace api
} // namespace vms
} // namespace nx
