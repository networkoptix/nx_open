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

enum class StreamQuality
{
    lowest = 0,
    low = 1,
    normal = 2,
    high = 3,
    highest = 4,
    preset = 5,
    undefined = 6
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreamQuality)

enum class FailoverPriority
{
    never = 0,
    low = 1,
    medium = 2,
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

enum class IoModuleVisualStyle
{
    form,
    tile
};

} // namespace api
} // namespace vms
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::CameraStatusFlags, (metatype)(numeric)(lexical),
    NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::CameraBackupQualities, (metatype)(numeric)(lexical),
    NX_VMS_API)
