#pragma once

#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace vms {
namespace api {

enum class ResourceStatus
{
    offline = 0,
    unauthorized = 1,
    online = 2,
    recording = 3,
    undefined = 4,
    incompatible = 5,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResourceStatus)

enum CameraStatusFlag
{
    CSF_NoFlags = 0x0,
    CSF_HasIssuesFlag = 0x1
};
Q_DECLARE_FLAGS(CameraStatusFlags, CameraStatusFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CameraStatusFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CameraStatusFlag)

enum class RecordingType
{
    /** Record always. */
    always = 0,

    /** Record only when motion was detected. */
    motionOnly = 1,

    /** Don't record. */
    never = 2,

    /** Record LQ stream always and HQ stream only on motion. */
    motionAndLow = 3,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RecordingType)
NX_VMS_API QString toString(RecordingType value);
NX_VMS_API std::ostream& operator<<(std::ostream& os, RecordingType value);

/**%apidoc Quality of the recording. */
enum class StreamQuality
{
    lowest = 0,
    low = 1,
    normal = 2,
    high = 3,
    highest = 4,
    preset = 5,
    undefined = 6,
    
    /**%apidoc[unused]
     * %// Used for rapid review only. The bitrate should be very high.
     */
    rapidReview = 7,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreamQuality)
NX_VMS_API QString toString(StreamQuality value);
NX_VMS_API std::ostream& operator<<(std::ostream& os, StreamQuality value);

/**%apidoc
 * Priority for the Camera to be moved to another Server for failover (if the current Server
 * fails).
 */
enum class FailoverPriority
{
    /**%apidoc Will never be moved to another Server.
     * %caption Never
     */
    never = 0,

    /**%apidoc Low priority against other Cameras.
     * %caption Low
     */
    low = 1,

    /**%apidoc Medium priority against other Cameras.
     * %caption Medium
     */
    medium = 2,

    /**%apidoc High priority against other cameras.
     * %caption High
     */
    high = 3,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FailoverPriority)

enum CameraBackupQuality
{
    CameraBackup_Disabled = 0,
    CameraBackup_HighQuality = 1,
    CameraBackup_LowQuality = 2,
    CameraBackup_Both = CameraBackup_HighQuality | CameraBackup_LowQuality,
    CameraBackup_Default = 4 // backup type didn't configured so far. Default value will be used
};
Q_DECLARE_FLAGS(CameraBackupQualities, CameraBackupQuality)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CameraBackupQuality)
Q_DECLARE_OPERATORS_FOR_FLAGS(CameraBackupQualities)

// TODO: #rvasilenko Write comments.
enum ServerFlag
{
    SF_None = 0x000,
    SF_Edge = 0x001,
    SF_RemoteEC = 0x002,
    SF_HasPublicIP = 0x004,
    SF_IfListCtrl = 0x008,
    SF_timeCtrl = 0x010,

    //* System name is default, so it will be displayed as "Unassigned System' in NxTool. */
    //SF_AutoSystemName = 0x020,

    SF_ArmServer = 0x040,
    SF_Has_HDD = 0x080,

    /** System is just installed, it has default admin password and is not linked to the cloud. */
    SF_NewSystem = 0x100,

    SF_SupportsTranscoding = 0x200,
    SF_HasLiteClient = 0x400,
    SF_P2pSyncDone = 0x1000000, //< For UT purpose only
    SF_RequiresEdgeLicense = 0x2000000, //< Remove when we are sure EDGE only licenses are gone.

    // Server can provide information about built-in PoE block.
    SF_HasPoeManagementCapability = 0x4000000,

    // Server can provide information about internal fan and its state.
    SF_HasFanMonitoringCapability = 0x8000000,

    // Server has a buzzer that can produce sound.
    SF_HasBuzzer = 0x10000000
};
Q_DECLARE_FLAGS(ServerFlags, ServerFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ServerFlags)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ServerFlag)

/**%apidoc
 * Settings for storage redundancy.
 */
enum class BackupType
{
    /**%apidoc
     * Backup is performed only at user's request.
     * %caption BackupManual
     */
    manual = 0,
    
    /**%apidoc
     * Backup is performed during recording.
     * %caption BackupRealTime
     */
    realtime = 1,

    /**%apidoc
     * Backup is performed according to the schedule.
     * %caption BackupSchedule
     */
    scheduled = 2
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(BackupType)

enum class IoModuleVisualStyle
{
    form,
    tile
};

enum class StreamDataFilter
{
    media = 1 << 0, //< Send media data.
    motion = 1 << 1, //< Send motion data.
    objectDetection = 1 << 2, //< Send analytics events.
};

Q_DECLARE_FLAGS(StreamDataFilters, StreamDataFilter)
Q_DECLARE_OPERATORS_FOR_FLAGS(StreamDataFilters)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreamDataFilter)

enum class MetadataStorageChangePolicy
{
    keep,
    remove,
    move,
};

} // namespace api
} // namespace vms
} // namespace nx

NX_VMS_API_DECLARE_TYPE(CameraStatusFlag)
NX_VMS_API_DECLARE_TYPE(CameraStatusFlags)
NX_VMS_API_DECLARE_TYPE(CameraBackupQuality)
NX_VMS_API_DECLARE_TYPE(CameraBackupQualities)
NX_VMS_API_DECLARE_TYPE(ServerFlag)
NX_VMS_API_DECLARE_TYPE(ServerFlags)
NX_VMS_API_DECLARE_TYPE(StreamDataFilter)
NX_VMS_API_DECLARE_TYPE(StreamDataFilters)
NX_VMS_API_DECLARE_TYPE(MetadataStorageChangePolicy)
