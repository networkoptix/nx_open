#pragma once

#include "resource_data.h"

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/literal.h>
#include <nx/utils/os_info.h>
#include <nx/vms/api/data_fwd.h>
#include <nx/vms/api/types/days_of_week.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API StorageData: ResourceData
{
    StorageData(): ResourceData(kResourceTypeId) {}

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    qint64 spaceLimit = 0;
    bool usedForWriting = false;
    QString storageType;
    ResourceParamDataList addParams;
    bool isBackup = false; /**< Whether the storage is used for backup. */
};
#define StorageData_Fields \
    ResourceData_Fields \
    (spaceLimit) \
    (usedForWriting) \
    (storageType) \
    (addParams) \
    (isBackup)

struct NX_VMS_API MediaServerData: ResourceData
{
    MediaServerData(): ResourceData(kResourceTypeId) {}

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    QString networkAddresses;
    ServerFlags flags = SF_None;
    QString version;
    QString systemInfo;
    QString authKey;
    QString osInfo;
};
#define MediaServerData_Fields \
    ResourceData_Fields \
    (networkAddresses) \
    (flags) \
    (version) \
    (systemInfo) \
    (authKey) \
    (osInfo)

struct NX_VMS_API MediaServerUserAttributesData: Data
{
    /**%apidoc Server unique id. */
    QnUuid serverId;

    /**%apidoc Server name. */
    QString serverName;

    /**%apidoc Maximum number of Cameras on the Server. */
    int maxCameras = 0;

    /**%apidoc
     * Whether the Server can take Cameras from an offline Server automatically.
     * %value false
     * %value true
     */
    bool allowAutoRedundancy = false;
    
    BackupType backupType = BackupType::manual;

    /**%apidoc:string
     * Days of week when the backup is allowed: the sum of values for each required day,
     * represented as a JSON string.
     */
    DaysOfWeek backupDaysOfTheWeek = DayOfWeek::all;

    /**%apidoc
     * Time of day when the backup starts (in seconds passed from 00:00:00).
     * %// It is an error if bDOW is set and this value is not set.
     */
    int backupStart = 0; 

    /**%apidoc Duration of the synchronization period (in seconds). -1 if not set. */
    int backupDuration = -1; //< Duration of synchronization period in seconds. -1 if not set.

    /**%apidoc Maximum backup bitrate (in bytes per second). Negative value if not limited. */
    int backupBitrate = kDefaultBackupBitrate;

    /**%apidoc Id of a Storage used to keep Analytics metadata database. */
    QnUuid metadataStorageId;

    static const int kDefaultBackupBitrate;

    QnUuid getIdForMerging() const { return serverId; } //< See IdData::getIdForMerging().

    static const DeprecatedFieldNames* getDeprecatedFieldNames();
};
#define MediaServerUserAttributesData_Fields_Short \
    (maxCameras) \
    (allowAutoRedundancy) \
    (backupType) \
    (backupDaysOfTheWeek) \
    (backupStart) \
    (backupDuration) \
    (backupBitrate) \
    (metadataStorageId)
#define MediaServerUserAttributesData_Fields \
    (serverId) \
    (serverName) \
    MediaServerUserAttributesData_Fields_Short

struct NX_VMS_API MediaServerDataEx:
    MediaServerData,
    MediaServerUserAttributesData
{
    ResourceStatus status = nx::vms::api::ResourceStatus::offline;
    ResourceParamDataList addParams;
    StorageDataList storages;

    MediaServerDataEx() = default;

    MediaServerDataEx(const MediaServerDataEx&) = default;
    MediaServerDataEx(MediaServerDataEx&&) = default;

    MediaServerDataEx& operator=(const MediaServerDataEx&) = default;
    MediaServerDataEx& operator=(MediaServerDataEx&&) = default;

    MediaServerDataEx(const MediaServerData& slice);
    MediaServerDataEx(MediaServerData&& slice);

    MediaServerDataEx& operator=(const MediaServerData& slice);
    MediaServerDataEx& operator=(MediaServerData&& slice);
};
#define MediaServerDataEx_Fields  \
    MediaServerData_Fields \
    MediaServerUserAttributesData_Fields_Short \
    (status) \
    (addParams) \
    (storages)

/**
 * Wrapper to be used for overloading as a distinct type for StorageData api requests.
 */
class StorageParentId: public QnUuid
{
public:
    StorageParentId() = default;
    StorageParentId(const QnUuid& id): QnUuid(id) {}
};

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::StorageData)
Q_DECLARE_METATYPE(nx::vms::api::StorageDataList)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerData)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerDataList)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerDataEx)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerDataExList)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerUserAttributesData)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerUserAttributesDataList)
