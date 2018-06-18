#pragma once

#include "resource_data.h"

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/literal.h>
#include <nx/vms/api/data_fwd.h>
#include <nx/vms/api/types/days_of_week.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API StorageData: ResourceData
{
    StorageData(): ResourceData(kStaticTypeId) {}

    static const QString kStaticTypeName;
    static const QnUuid kStaticTypeId;

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
    MediaServerData(): ResourceData(kStaticTypeId) {}

    static const QString kStaticTypeName;
    static const QnUuid kStaticTypeId;

    QString networkAddresses;
    ServerFlags flags = SF_None;
    QString version;
    QString systemInfo;
    QString authKey;
};
#define MediaServerData_Fields \
    ResourceData_Fields \
    (networkAddresses) \
    (flags) \
    (version) \
    (systemInfo) \
    (authKey)

struct NX_VMS_API MediaServerUserAttributesData: Data
{
    QnUuid serverId;
    QString serverName;
    int maxCameras = 0;
    bool allowAutoRedundancy = false; //< Server can take cameras from offline server automatically.

    // Redundant storage settings.
    BackupType backupType = BackupType::manual;

    // Days of week mask.
    DaysOfWeek backupDaysOfTheWeek = DayOfWeek::all;

    int backupStart = 0; //< Seconds from 00:00:00. Error if bDOW set and this is not set.
    int backupDuration = -1; //< Duration of synchronization period in seconds. -1 if not set.

    // Bitrate cap in bytes per second. Negative value if not capped. Not capped by default.
    int backupBitrate = kDefaultBackupBitrate;
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
    (backupBitrate)
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
    MediaServerDataEx(const MediaServerDataEx& mediaServerData);
    MediaServerDataEx(MediaServerData&& mediaServerData);
    MediaServerDataEx(MediaServerDataEx&& mediaServerData);

    MediaServerDataEx& operator=(const MediaServerDataEx&) = default;
    MediaServerDataEx& operator=(MediaServerDataEx&& mediaServerData);
};
#define MediaServerDataEx_Fields  \
    MediaServerData_Fields \
    MediaServerUserAttributesData_Fields_Short \
    (status) \
    (addParams) \
    (storages)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::StorageData)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerData)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerDataEx)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerUserAttributesData)
