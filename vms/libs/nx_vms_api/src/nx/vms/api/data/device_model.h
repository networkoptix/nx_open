// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <type_traits>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/data/device_media_stream_info.h>
#include <nx/vms/api/data/media_stream_capability.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types/device_type.h>
#include <nx/vms/api/types/ptz_types.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/credentials.h>
#include <nx/vms/api/data/device_profile.h>

namespace nx::vms::api {

struct DeviceBitrateInfo
{
    StreamIndex encoderIndex = StreamIndex::undefined;

    std::chrono::milliseconds timestampMs;

    /**%apidoc[readonly] Megabits per second. */
    float rawSuggestedBitrate = -1;

    /**%apidoc[readonly] Megabits per second. */
    float suggestedBitrate = -1;

    /**%apidoc[readonly] Megabits per second. Current bitrate value. */
    float actualBitrate = -1;

    bool bitratePerGop = false;
    float bitrateFactor = -1;

    int fps = -1;
    float actualFps = -1;
    float averageGopSize = 0;
    QString resolution;
    int numberOfChannels = -1;
    bool isConfigured = false;

    /**%apidoc[readonly] Megabits per second. Average bitrate for a whole archive. */
    float avarageBitrateMbps = -1;
};
#define DeviceBitrateInfo_Fields (encoderIndex)(timestampMs) \
    (rawSuggestedBitrate)(suggestedBitrate)(actualBitrate) \
    (bitratePerGop)(bitrateFactor) \
    (fps)(actualFps)(averageGopSize)(resolution)(numberOfChannels)(isConfigured)(avarageBitrateMbps)
QN_FUSION_DECLARE_FUNCTIONS(DeviceBitrateInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceBitrateInfo, DeviceBitrateInfo_Fields)

NX_REFLECTION_ENUM_CLASS(IoSettingsPortType,
    unknown = 0x0,
    disabled = 0x1 << 0,
    input = 0x1 << 1,
    output = 0x1 << 2,

    PT_Unknown = IoSettingsPortType::unknown,
    PT_Disabled = IoSettingsPortType::disabled,
    PT_Input = IoSettingsPortType::input,
    PT_Output = IoSettingsPortType::output
)
Q_DECLARE_FLAGS(IoSettingsPortTypes, IoSettingsPortType)
Q_DECLARE_OPERATORS_FOR_FLAGS(IoSettingsPortTypes)

enum class IoSettingsDefaultState
{
    open_circuit,
    grounded_circuit,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(IoSettingsDefaultState*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<IoSettingsDefaultState>;
    return visitor(Item{IoSettingsDefaultState::open_circuit, "open_circuit"},
        Item{IoSettingsDefaultState::open_circuit, "Open Circuit"},
        Item{IoSettingsDefaultState::grounded_circuit, "grounded_circuit"},
        Item{IoSettingsDefaultState::grounded_circuit, "Grounded Circuit"});
}

struct IoSettings
{
    QString id;
    IoSettingsPortType portType = IoSettingsPortType::disabled;
    IoSettingsPortTypes supportedPortTypes;
    QString inputName;
    QString outputName;
    IoSettingsDefaultState iDefaultState = IoSettingsDefaultState::open_circuit;
    IoSettingsDefaultState oDefaultState = IoSettingsDefaultState::open_circuit;

    /**%apidoc
     * For output only. Keep output state on during timeout if non zero.
     * %example 0
     */
    std::chrono::milliseconds autoResetTimeoutMs{0};
};
#define IoSettings_Fields (id)(portType) \
    (supportedPortTypes)(inputName)(outputName) \
    (iDefaultState)(oDefaultState) \
    (autoResetTimeoutMs)
QN_FUSION_DECLARE_FUNCTIONS(IoSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(IoSettings, IoSettings_Fields)

struct DeviceGroupSettings
{
    QString id;

    /**%apidoc
     * %example Group 1
     */
    QString name;
    bool operator==(const DeviceGroupSettings& other) const = default;
};
#define DeviceGroupSettings_Fields (id)(name)
QN_FUSION_DECLARE_FUNCTIONS(DeviceGroupSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceGroupSettings, DeviceGroupSettings_Fields)

/**%apidoc Device information object.
 * %param[opt]:object parameters Device specific key-value parameters.
 */
struct NX_VMS_API DeviceModelGeneral: ResourceWithParameters
{
    /**%apidoc Depends on `physicalId`. Calculated automatically by default. */
    nx::Uuid id;

    /**%apidoc
     * Depends on the Device hardware and firmware. Unique per device.
     * %example 92-61-00-00-00-9F
     */
    QString physicalId;

    /**%apidoc
     * %example 192.168.0.1
     */
    QString url;

    /**%apidoc
     * Device Driver id, can be obtained from <code>GET /rest/v{1-}/devices/&ast;/types</code>.
     * %example 1b7181ce-0227-d3f7-9443-c86aab922d96
     */
    nx::Uuid typeId;

    /**%apidoc[opt]
     * %example Device 1
     */
    QString name;

    /**%apidoc[opt] */
    QString mac;

    /**%apidoc[opt]
     * Parent Server, can be obtained from `GET /rest/v{1-}/servers`, current Server by default.
     */
    nx::Uuid serverId;

    /**%apidoc[opt] */
    bool isManuallyAdded = false;

    /**%apidoc[immutable] Device manufacturer. */
    QString vendor;

    /**%apidoc[immutable] Device model. */
    QString model;

    std::optional<DeviceGroupSettings> group;

    /**%apidoc The password is only visible for admins if exposeDeviceCredentials option is enabled
     * in the Site settings.
     */
    std::optional<Credentials> credentials;

    static DeviceModelGeneral fromCameraData(CameraData data);
    CameraData toCameraData() &&;

    using DbReadTypes = std::tuple<CameraData,
        CameraAttributesData,
        ResourceStatusDataList,
        ResourceParamWithRefDataList>;

    using DbUpdateTypes = std::tuple<CameraData,
        std::optional<CameraAttributesData>,
        std::optional<ResourceStatusData>,
        ResourceParamWithRefDataList>;

    using DbListTypes = std::tuple<CameraDataList,
        CameraAttributesDataList,
        ResourceStatusDataList,
        ResourceParamWithRefDataList>;
};
#define DeviceModelGeneral_Fields ResourceWithParameters_Fields \
    (id)(name)(url)(typeId)(mac)(serverId)(physicalId) \
    (isManuallyAdded)(vendor)(model)(group)(credentials)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(DeviceModelGeneral, (json))
NX_REFLECTION_INSTRUMENT(DeviceModelGeneral, DeviceModelGeneral_Fields);

/**%apidoc
 * %param[unused]:object parameters Device specific key-value parameters.
 */
struct NX_VMS_API DeviceModelForSearch: DeviceModelGeneral
{
    /**%apidoc Whether Device was already found. */
    bool wasAlreadyFound = false;
};
#define DeviceModelForSearch_Fields DeviceModelGeneral_Fields(wasAlreadyFound)
QN_FUSION_DECLARE_FUNCTIONS(DeviceModelForSearch, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceModelForSearch, DeviceModelForSearch_Fields);

struct DeviceOptions
{
    /**%apidoc[opt] */
    bool isControlEnabled = true;

    /**%apidoc[opt] */
    bool isAudioEnabled = false;

    /**%apidoc[opt] */
    bool isDualStreamingDisabled = false;

    /**%apidoc[opt] */
    QString dewarpingParams;

    /**%apidoc[opt] */
    nx::Uuid preferredServerId;

    /**%apidoc[opt] */
    FailoverPriority failoverPriority = FailoverPriority::medium;

    /**%apidoc[opt] Combination (via "|") of the flags defining backup options. */
    CameraBackupQuality backupQuality = CameraBackupDefault;

    /**%apidoc[opt] */
    BackupContentTypes backupContentType = BackupContentType::archive; //< What to backup content wise.

    /**%apidoc[opt] */
    BackupPolicy backupPolicy = BackupPolicy::byDefault;

    std::optional<nx::Uuid> audioOutputDeviceId;

    /**%apidoc[readonly] */
    std::optional<std::vector<DeviceBitrateInfo>> bitrateInfos;

    std::optional<bool> useBitratePerGop;

    std::optional<bool> cameraHotspotsEnabled;

    std::optional<bool> dontRecordSecondaryStream;

    std::optional<bool> forcedMotionDetection;

    std::optional<IoModuleVisualStyle> ioOverlayStyle;

    // TODO: #skolesnik rename `QnVirtualCameraResource::motionStreamIndexInternal()`
    std::optional<StreamIndex> motionStream;

    std::optional<std::vector<IoSettings>> ioSettings;

    std::optional<int> mediaPort;

    /**%apidoc[readonly] */
    std::optional<bool> hasRtspSettings;
};
#define DeviceOptions_Fields \
    (isControlEnabled)(isAudioEnabled)(isDualStreamingDisabled) \
    (dewarpingParams)(preferredServerId)(failoverPriority)(backupQuality)(backupContentType) \
    (backupPolicy)(audioOutputDeviceId)(bitrateInfos)(useBitratePerGop) \
    (cameraHotspotsEnabled)(dontRecordSecondaryStream)(forcedMotionDetection) \
    (ioOverlayStyle)(motionStream)(ioSettings)(mediaPort)(hasRtspSettings)
QN_FUSION_DECLARE_FUNCTIONS(DeviceOptions, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceOptions, DeviceOptions_Fields);

struct DeviceScheduleSettings
{
    bool isEnabled = false;
    std::vector<CameraScheduleTaskData> tasks;

    /**%apidoc Minimum number of days to keep the archive for.
     * %deprecated This fields is deprecated and used for backward compatibility only. Use
     *     'minArchivePeriodS' instead.
     */
    std::optional<int> minArchiveDays;

    /**%apidoc[opt] Maximum number of days to keep the archive for.
     * %deprecated This fields is deprecated and used for backward compatibility only. Use
     *     'maxArchivePeriodS' instead.
     */
    std::optional<int> maxArchiveDays;

    /**%apidoc[opt] Minimum number of seconds to keep the archive for. If the value is less than or
     * equal to zero, it is not used.
     */
    std::optional<int> minArchivePeriodS;

    /**%apidoc[opt] Maximum number of seconds to keep the archive for. If the value is less than or
     * equal zero, it is not used.
     */
    std::optional<int> maxArchivePeriodS;
};
#define DeviceScheduleSettings_Fields (isEnabled)(tasks)(minArchiveDays)(maxArchiveDays)(minArchivePeriodS)(maxArchivePeriodS)
QN_FUSION_DECLARE_FUNCTIONS(DeviceScheduleSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceScheduleSettings, DeviceScheduleSettings_Fields);

struct DeviceMotionSettings
{
    MotionType type = MotionType::default_;

    QString mask;

    /**%apidoc[opt] */
    int recordBeforeS = kDefaultRecordBeforeMotionSec;

    /**%apidoc[opt] */
    int recordAfterS = kDefaultRecordBeforeMotionSec;
};
#define DeviceMotionSettings_Fields (type)(mask)(recordBeforeS)(recordAfterS)
QN_FUSION_DECLARE_FUNCTIONS(DeviceMotionSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceMotionSettings, DeviceMotionSettings_Fields);

/** ATTENTION: When changing/adding values, adjust apidoc for /ec2/getResourceParams. */
NX_REFLECTION_ENUM_CLASS(DeviceCapability,
    noCapabilities = 0,
    // 1 << 0 is skipped.
    // 1 << 1 is skipped.

    /**%apidoc The Device primary stream is suitable for software motion detection. */
    primaryStreamSoftMotion = 1 << 2,

    /**%apidoc The Device has input port(s). */
    inputPort = 1 << 3,

    /**%apidoc The Device has output port(s). */
    outputPort = 1 << 4,

    /**%apidoc Whether multiple instances on the same IP address are allowed. */
    shareIp = 1 << 5,

    /**%apidoc The Device has a Two-Way Audio support. */
    audioTransmit = 1 << 6,

    // 1 << 7 is skipped.

    /**%apidoc The Device supports remote archiving. */
    remoteArchive = 1 << 8,

    /**%apidoc Password on the Device can be changed. */
    setUserPassword = 1 << 9,

    /**%apidoc The Device has the default password now. */
    isDefaultPassword = 1 << 10,

    /**%apidoc The Device firmware is too old. */
    isOldFirmware = 1 << 11,

    /**%apidoc The Device `streamUrls` field is editable. */
    customMediaUrl = 1 << 12,

    /**%apidoc The Device is an NVR which supports playback speeds 1, 2, 4, etc. natively. */
    isPlaybackSpeedSupported = 1 << 13,

    /**%apidoc
     * The Device is one of the NVR channels that depend on each other and can be played back only
     * synchronously.
     */
    synchronousChannel = 1 << 14,

    dualStreamingForLiveOnly = 1 << 15,

    /**%apidoc The ports of the Device media streams are editable. */
    customMediaPorts = 1 << 16,

    /**%apidoc The Device sends absolute timestamps in the media stream. */
    absoluteTimestamps = 1 << 17,

    /**%apidoc The Device does not allow to change stream quality/FPS. */
    fixedQuality = 1 << 18,

    /**%apidoc The Device supports multicast streaming. */
    multicastStreaming = 1 << 19,

    /**%apidoc The Device is bound to a particular Server. */
    boundToServer = 1 << 20,

    /**%apidoc The Server should not open video from the Device at its will. */
    dontAutoOpen = 1 << 21,

    /**%apidoc Analytics Engines should not bind to the Device. */
    noAnalytics = 1 << 22,

    /**%apidoc The Device is initialized as an ONVIF device. */
    isOnvif = 1 << 23,

    /**%apidoc The Device is an intercom. */
    isIntercom = 1 << 24
)
Q_DECLARE_FLAGS(DeviceCapabilities, DeviceCapability)

struct NX_VMS_API PanTiltSensitivity
{
    /**%apidoc
     * Ptz pan sensitivity, the value is between 0.1 and 1.0
     * %example 1.0
     */
    qreal pan = nx::vms::api::ptz::kDefaultSensitivity;

    /**%apidoc
     * Ptz tilt sensitivity, the value is between 0.1 and 1.0
     * %example 1.0
     */
    qreal tilt = nx::vms::api::ptz::kDefaultSensitivity;
};
#define PanTiltSensitivity_Fields (pan)(tilt)
QN_FUSION_DECLARE_FUNCTIONS(PanTiltSensitivity, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PanTiltSensitivity, PanTiltSensitivity_Fields);

struct NX_VMS_API DevicePtzOptions
{
    /**%apidoc
     * Ptz uniform pan and tilt sensitivity or separate sensitivities. Values are between 0.1 and 1.0.
     * %example 1.0
     */
    std::optional<std::variant<qreal, PanTiltSensitivity>> panTiltSensitivity;

    /**apidoc Preferred preset type */
    std::optional<nx::vms::api::ptz::PresetType> presetType;

    /**%apidoc[readonly] Effective capabilities for use in `/rest/v{4-}/devices/{id}/ptz/`. */
    nx::vms::api::ptz::Capabilities capabilities;

    /**%apidoc[readonly] */
    std::optional<nx::vms::api::ptz::Capabilities> configCapabilities;

    /**%apidoc[readonly] Extra capabilities which can be added by `userAddedCapabilities`. */
    std::optional<nx::vms::api::ptz::Capabilities> userModifiableCapabilities;

    std::optional<nx::vms::api::ptz::Capabilities> userAddedCapabilities;
};
#define DevicePtzOptions_Fields (panTiltSensitivity)(presetType)(capabilities) \
    (configCapabilities)(userModifiableCapabilities)(userAddedCapabilities)
QN_FUSION_DECLARE_FUNCTIONS(DevicePtzOptions, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DevicePtzOptions, DevicePtzOptions_Fields);

struct NX_VMS_API DeviceModelDeprecated
{
    /**%apidoc[opt] */
    CameraBackupQuality backupQuality = CameraBackupDefault;
};
#define DeviceModelDeprecated_Fields (backupQuality)

struct NX_VMS_API DeviceModelV1Base: DeviceModelGeneral
{
    /**%apidoc[opt] */
    QString logicalId;

    /**%apidoc[opt] */
    DeviceOptions options;

    /**%apidoc[opt] */
    DeviceScheduleSettings schedule;

    /**%apidoc[opt] */
    DeviceMotionSettings motion;

    /**%apidoc[readonly] */
    std::optional<ResourceStatus> status;

    /**%apidoc[opt] */
    bool isLicenseUsed = false;

    /**%apidoc[immutable] */
    std::optional<DeviceCapabilities> capabilities;

    /**%apidoc[immutable]
     * %// Appeared starting from /rest/v2/devices.
     */
    std::optional<DeviceType> deviceType;
};
#define DeviceModelV1Base_Fields DeviceModelGeneral_Fields \
    (logicalId)(options)(schedule) (motion) (status) (isLicenseUsed) \
    (capabilities) (deviceType)

struct NX_VMS_API DeviceModelV1: DeviceModelV1Base, DeviceModelDeprecated
{
    DbUpdateTypes toDbTypes() &&;
    static std::vector<DeviceModelV1> fromDbTypes(DbListTypes data);
};
#define DeviceModelV1_Fields DeviceModelV1Base_Fields DeviceModelDeprecated_Fields
QN_FUSION_DECLARE_FUNCTIONS(DeviceModelV1, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceModelV1, DeviceModelV1_Fields);

struct NX_VMS_API DeviceModelV3Base: DeviceModelV1Base
{
    /**%apidoc[readonly] */
    std::vector<nx::Uuid> compatibleAnalyticsEngineIds;

    /**%apidoc[readonly] */
    CameraMediaCapability mediaCapabilities;

    /**%apidoc[readonly] */
    std::vector<DeviceMediaStreamInfo> mediaStreams;

    /**%apidoc
     * The Device stream urls. It can be set only if the field `capabilities`
     * has `customMediaUrl` flag.
     */
    std::optional<QJsonObject> streamUrls;

    // TODO: #skolesnik
    //     Fix the bug which prevents from creating a new camera with a non-`std::nullopt` value.
    //     The crash also happens when creating a new camera passing `userEnabledAnalyticsEngines`
    //     (empty or not) in `parameters` via `v{1-2}`.
    //     PATCH (or PUT with existing) works fine.
    std::optional<std::vector<nx::Uuid>> userEnabledAnalyticsEngineIds;
};
#define DeviceModelV3Base_Fields DeviceModelV1Base_Fields \
    (compatibleAnalyticsEngineIds)(mediaCapabilities)(mediaStreams) \
    (streamUrls)(userEnabledAnalyticsEngineIds)

struct NX_VMS_API DeviceModelV3: DeviceModelV3Base, DeviceModelDeprecated
{
    DbUpdateTypes toDbTypes() &&;
    static std::vector<DeviceModelV3> fromDbTypes(DbListTypes data);
};
#define DeviceModelV3_Fields DeviceModelV3Base_Fields DeviceModelDeprecated_Fields
QN_FUSION_DECLARE_FUNCTIONS(DeviceModelV3, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceModelV3, DeviceModelV3_Fields);

struct NX_VMS_API DeviceModelV4: DeviceModelV3Base
{
    std::optional<DevicePtzOptions> ptz;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<DeviceModelV4> fromDbTypes(DbListTypes data);
};
#define DeviceModelV4_Fields DeviceModelV3Base_Fields \
    (ptz)
QN_FUSION_DECLARE_FUNCTIONS(DeviceModelV4, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceModelV4, DeviceModelV4_Fields);

struct NX_VMS_API DeviceTypeModel
{
    nx::Uuid id;
    std::optional<nx::Uuid> parentId;
    QString name;
    QString manufacturer;
};
#define DeviceTypeModel_Fields (id)(parentId)(name)(manufacturer)
QN_FUSION_DECLARE_FUNCTIONS(DeviceTypeModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceTypeModel, DeviceTypeModel_Fields);

using DeviceModel = DeviceModelV4;

} // namespace nx::vms::api
