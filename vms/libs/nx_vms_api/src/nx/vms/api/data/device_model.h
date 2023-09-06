// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <type_traits>

#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/api/types/device_type.h>

#include "camera_attributes_data.h"
#include "camera_data.h"
#include "credentials.h"
#include "resource_data.h"
#include "type_traits.h"

namespace nx::vms::api {

struct DeviceGroupSettings
{
    QString id;

    /**%apidoc
     * %example Group 1
     */
    QString name;
};
#define DeviceGroupSettings_Fields (id)(name)
QN_FUSION_DECLARE_FUNCTIONS(DeviceGroupSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceGroupSettings, DeviceGroupSettings_Fields);

struct NX_VMS_API DeviceModelGeneral
{
    /**%apidoc Depends on `physicalId`. Calculated automatically by default. */
    QnUuid id;

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
     * Device Driver id, can be obtained from `GET /rest/v{1-}/devices/\*\/types`.
     * %example 1b7181ce-0227-d3f7-9443-c86aab922d96
     */
    QnUuid typeId;

    /**%apidoc[opt]
     * %example Device 1
     */
    QString name;

    /**%apidoc[opt] */
    QString mac;

    /**%apidoc[opt]
     * Parent Server, can be obtained from `GET /rest/v{1-}/servers`, current Server by default.
     */
    QnUuid serverId;

    /**%apidoc[opt] */
    bool isManuallyAdded = false;

    /**%apidoc[readonly] Device manufacturer. */
    QString vendor;

    /**%apidoc[readonly] Device model. */
    QString model;

    std::optional<DeviceGroupSettings> group;

    /**%apidoc The password is only visible for admins if exposeDeviceCredentials option is enabled
     * in the System settings.
     */
    std::optional<Credentials> credentials;

    DeviceModelGeneral() = default;
    explicit DeviceModelGeneral(CameraData data);

    CameraData toCameraData() &&;
};
#define DeviceModelGeneral_Fields \
    (id)(name)(url)(typeId)(mac)(serverId)(physicalId)(isManuallyAdded)(vendor)(model)(group)(credentials)
QN_FUSION_DECLARE_FUNCTIONS(DeviceModelGeneral, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceModelGeneral, DeviceModelGeneral_Fields);

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
    QnUuid preferredServerId;

    /**%apidoc[opt] */
    FailoverPriority failoverPriority = FailoverPriority::medium;

    /**%apidoc[opt] Combination (via "|") of the flags defining backup options. */
    CameraBackupQuality backupQuality = CameraBackupDefault;

    /**%apidoc[opt] */
    BackupContentTypes backupContentType = BackupContentType::archive; //< What to backup content wise.

    /**%apidoc[opt] */
    BackupPolicy backupPolicy = BackupPolicy::byDefault;
};
#define DeviceOptions_Fields \
    (isControlEnabled)(isAudioEnabled)(isDualStreamingDisabled) \
    (dewarpingParams)(preferredServerId)(failoverPriority)(backupQuality)(backupContentType) \
    (backupPolicy)
QN_FUSION_DECLARE_FUNCTIONS(DeviceOptions, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceOptions, DeviceOptions_Fields);

struct DeviceScheduleSettings
{
    bool isEnabled = false;
    std::vector<ScheduleTaskData> tasks;

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

    /**%apidoc The Device streams are editable. */
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


// TODO #lbusygin: Remove 'isLicenseUsed' field in v2, it is duplicated by 'schedule.enabled'.
/**%apidoc Device information object.
 * %param[opt]:object parameters Device specific key-value parameters.
 */
struct NX_VMS_API DeviceModel: DeviceModelGeneral, ResourceWithParameters
{
    /**%apidoc[opt] */
    QString logicalId;

    /**%apidoc[opt] */
    DeviceOptions options;

    /**%apidoc[opt] */
    DeviceScheduleSettings schedule;

    /**%apidoc[opt] */
    DeviceMotionSettings motion;

    std::optional<ResourceStatus> status;

    /**%apidoc[opt] */
    bool isLicenseUsed = false;

    /**%apidoc[opt] */
    CameraBackupQuality backupQuality = CameraBackupDefault;

    /**%apidoc[readonly]
     * %// Appeared starting from /rest/v2/devices.
     */
    std::optional<DeviceType> deviceType;

    /**%apidoc[readonly] */
    std::optional<DeviceCapabilities> capabilities;

    DeviceModel() = default;
    explicit DeviceModel(CameraData cameraData);

    using DbReadTypes = std::tuple<
        CameraData,
        CameraAttributesData,
        ResourceStatusDataList,
        ResourceParamWithRefDataList
    >;

    using DbUpdateTypes = std::tuple<
        CameraData,
        std::optional<CameraAttributesData>,
        std::optional<ResourceStatusData>,
        ResourceParamWithRefDataList
    >;

    using DbListTypes = std::tuple<
        CameraDataList,
        CameraAttributesDataList,
        ResourceStatusDataList,
        ResourceParamWithRefDataList
    >;

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }
    static_assert(isCreateModelV<DeviceModel>);
    static_assert(isUpdateModelV<DeviceModel>);
    static_assert(isFlexibleIdModelV<DeviceModel>);

    DbUpdateTypes toDbTypes() &&;
    static std::vector<DeviceModel> fromDbTypes(DbListTypes data);

    void extractFromList(const QnUuid& id, ResourceParamWithRefDataList* list);
};
#define DeviceModel_Fields DeviceModelGeneral_Fields \
    (parameters)(logicalId)(status)(options)(schedule)(motion)(isLicenseUsed)(backupQuality) \
    (deviceType)(capabilities)
QN_FUSION_DECLARE_FUNCTIONS(DeviceModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceModel, DeviceModel_Fields);

struct NX_VMS_API DeviceTypeModel
{
    QnUuid id;
    std::optional<QnUuid> parentId;
    QString name;
    QString manufacturer;
};
#define DeviceTypeModel_Fields (id)(parentId)(name)(manufacturer)
QN_FUSION_DECLARE_FUNCTIONS(DeviceTypeModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceTypeModel, DeviceTypeModel_Fields);

} // namespace nx::vms::api
