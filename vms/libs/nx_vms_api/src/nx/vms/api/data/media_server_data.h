// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_data.h"

#include <QtCore/QString>

#include <nx/vms/api/types/day_of_week.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>

namespace nx::vms::api {

/**apidoc
 * %param[opt] id Storage unique id. Can be omitted when creating a new object.
 * %param parentId Parent Server unique id.
 * %param[opt] name Arbitrary Storage name.
 * %param url Full Storage url (path to the local folder).
 * %param[proprietary] typeId Should have fixed value.
 *     %value {f8544a40-880e-9442-b78a-9da6db6862b4}
 */
struct NX_VMS_API StorageData: ResourceData
{
    StorageData(): ResourceData(kResourceTypeId) {}

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    /**%apidoc[opt]
     * Free space to maintain on the Storage, in bytes. Recommended value is 10 gigabytes for local
     * Storages and 100 gigabytes for NAS.
     */
    qint64 spaceLimit = -1;

    /**%apidoc[opt] Whether writing to the Storage is allowed. */
    bool usedForWriting = false;

    /**%apidoc[opt]
     * Type of the method to access the Storage.
     *     %value "local"
     *     %value "network" Manually mounted NAS.
     *     %value "smb" Automatically mounted NAS.
     */
    QString storageType;

    /**apidoc[proprietary]
     * List of Storage additional parameters. Intended for internal use; leave empty when creating
     * a new Storage.
     */
    ResourceParamDataList addParams;
    ResourceStatus status = ResourceStatus::offline;

    /**%apidoc[opt] Whether the Storage is used for backup. */
    bool isBackup = false;

    bool operator==(const StorageData& other) const = default;
};
#define StorageData_Fields \
    ResourceData_Fields \
    (spaceLimit) \
    (usedForWriting) \
    (storageType) \
    (addParams) \
    (status) \
    (isBackup)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(StorageData)
NX_REFLECTION_INSTRUMENT(StorageData, StorageData_Fields)

struct NX_VMS_API MediaServerData: ResourceData
{
    MediaServerData(): ResourceData(kResourceTypeId) {}

    bool operator==(const MediaServerData& other) const = default;

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
NX_VMS_API_DECLARE_STRUCT_AND_LIST(MediaServerData)
NX_REFLECTION_INSTRUMENT(MediaServerData, MediaServerData_Fields)

struct NX_VMS_API BackupBitrateKey
{
    DayOfWeek day = DayOfWeek::monday;
    int hour = 0;

    bool operator<(const BackupBitrateKey& other) const
    {
        if (day != other.day)
            return day < other.day;
        return hour < other.hour;
    }
};
#define BackupBitrateKey_Fields \
    (day) \
    (hour)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(BackupBitrateKey)
NX_REFLECTION_INSTRUMENT(BackupBitrateKey, BackupBitrateKey_Fields)

using BackupBitrateBytesPerSecond = QMap<BackupBitrateKey, qint64>;
constexpr int64_t kNoRecordingBitrateBps = 0L;

void NX_VMS_API serialize(
    nx::reflect::json::SerializationContext* context,
    const BackupBitrateBytesPerSecond& value);
nx::reflect::DeserializationResult NX_VMS_API deserialize(
    const nx::reflect::json::DeserializationContext& context,
    BackupBitrateBytesPerSecond* value);

/**%apidoc
 * %param[opt]:array backupBitrateBytesPerSecond
 *     Backup bitrate per day of week and hour, as a JSON array of key-value objects, structured
 *     according to the following example:
 *     <pre><code>
 *     [
 *         {
 *             "key": { "day": "DAY_OF_WEEK", "hour": HOUR },
 *             "value": BYTES_PER_SECOND
 *         },
 *         ...
 *     ]
 *     </code></pre>
 *     Here <code>DAY_OF_WEEK</code> is one of <code>monday</code>, <code>tuesday</code>,
 *     <code>wednesday</code>, <code>thursday</code>, <code>friday</code>, <code>saturday</code>,
 *     <code>sunday</code>; <code>HOUR</code> is an integer in range 0..23;
 *     <code>BYTES_PER_SECOND</code> is an integer that can be represented as a number or a string.
 *     <br/>
 *     For any day-hour position, a missing value means "unlimited bitrate", and a zero value means
 *     "don't perform backup".
 * %param:object backupBitrateBytesPerSecond[].key
 * %param:enum backupBitrateBytesPerSecond[].key.day
 *     %value monday
 *     %value tuesday
 *     %value wednesday
 *     %value thursday
 *     %value friday
 *     %value saturday
 *     %value sunday
 * %param:integer backupBitrateBytesPerSecond[].key.hour
 *     %example 0
 * %param:string backupBitrateBytesPerSecond[].value
 *     %example 0
 */
struct NX_VMS_API MediaServerUserAttributesData
{
    /**%apidoc Server unique id. */
    QnUuid serverId;

    /**%apidoc Server name. */
    QString serverName;

    /**%apidoc Maximum number of cameras on the server. */
    int maxCameras = 0;

    /**%apidoc
     * Whether the Server can take Cameras from an offline Server automatically.
     * %value false
     * %value true
     */
    bool allowAutoRedundancy = false;

    BackupBitrateBytesPerSecond backupBitrateBytesPerSecond;
    int locationId = 0;

    /** Used by ...Model::toDbTypes() and transaction-description-modify checkers. */
    CheckResourceExists checkResourceExists = CheckResourceExists::yes; /**<%apidoc[unused] */

    QnUuid getIdForMerging() const { return serverId; } //< See IdData::getIdForMerging().
    QnUuid getId() const { return serverId; }

    bool operator==(const MediaServerUserAttributesData& other) const = default;

    static const DeprecatedFieldNames* getDeprecatedFieldNames();
};
#define MediaServerUserAttributesData_Fields_Short \
    (maxCameras) \
    (allowAutoRedundancy) \
    (backupBitrateBytesPerSecond)\
    (locationId)

#define MediaServerUserAttributesData_Fields \
    (serverId) \
    (serverName) \
    MediaServerUserAttributesData_Fields_Short
NX_VMS_API_DECLARE_STRUCT_AND_LIST(MediaServerUserAttributesData)
NX_REFLECTION_INSTRUMENT(MediaServerUserAttributesData, MediaServerUserAttributesData_Fields)

struct NX_VMS_API MediaServerDataEx:
    MediaServerData,
    MediaServerUserAttributesData
{
    ResourceStatus status = ResourceStatus::offline;
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

    bool operator==(const MediaServerDataEx& other) const = default;
};
#define MediaServerDataEx_Fields  \
    MediaServerData_Fields \
    MediaServerUserAttributesData_Fields_Short \
    (status) \
    (addParams) \
    (storages)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(MediaServerDataEx)
NX_REFLECTION_INSTRUMENT(MediaServerDataEx, MediaServerDataEx_Fields)

/**
 * Wrapper to be used for overloading as a distinct type for StorageData api requests.
 */
class StorageParentId: public QnUuid
{
public:
    StorageParentId() = default;
    StorageParentId(const QnUuid& id): QnUuid(id) {}
    StorageParentId(const QString& id): QnUuid(id) {}
};

struct StorageFilter: IdData
{
    using IdData::IdData;
    StorageFilter(const QnUuid& id, const QnUuid& serverId): IdData(id), serverId(serverId) {}
    StorageFilter getId() const { return *this; }
    bool operator==(const StorageFilter& origin) const = default;

    QnUuid serverId;
};

QN_FUSION_DECLARE_FUNCTIONS(StorageFilter, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::StorageData)
Q_DECLARE_METATYPE(nx::vms::api::StorageDataList)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerData)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerDataList)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerDataEx)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerDataExList)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerUserAttributesData)
Q_DECLARE_METATYPE(nx::vms::api::MediaServerUserAttributesDataList)
