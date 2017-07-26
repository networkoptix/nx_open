#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "api_globals.h"
#include "api_resource_data.h"
#include <core/resource/resource_type.h>

namespace ec2
{
    namespace backup
    {
        // TODO: #rvasilenko move out to more general space
        enum DayOfWeek
        {
            Never       = 0x00,
            Monday      = 0x01,
            Tuesday     = 0x02,
            Wednesday   = 0x04,
            Thursday    = 0x08,
            Friday      = 0x10,
            Saturday    = 0x20,
            Sunday      = 0x40,

            AllDays     = 0x7F
        };
        Q_DECLARE_FLAGS(DaysOfWeek, DayOfWeek)
        Q_DECLARE_OPERATORS_FOR_FLAGS(DaysOfWeek)

        backup::DayOfWeek fromQtDOW(Qt::DayOfWeek qtDOW);
        backup::DaysOfWeek fromQtDOW(QList<Qt::DayOfWeek> days);
    }

    struct ApiStorageData: ApiResourceData
    {
        ApiStorageData():
            spaceLimit(0),
            usedForWriting(0),
            isBackup(false)
        {
            typeId = QnResourceTypePool::kStorageTypeUuid;
        }

        qint64          spaceLimit;
        bool            usedForWriting;
        QString         storageType;
        std::vector<ApiResourceParamData> addParams;
        bool            isBackup;              // is storage used for backup
    };
#define ApiStorageData_Fields   \
    ApiResourceData_Fields      \
    (spaceLimit)                \
    (usedForWriting)            \
    (storageType)               \
    (addParams)                 \
    (isBackup)


    struct ApiMediaServerData: ApiResourceData
    {
        ApiMediaServerData():
            flags(Qn::SF_None)
        {
            typeId = QnResourceTypePool::kServerTypeUuid;
        }

        QString         networkAddresses;
        Qn::ServerFlags flags;
        QString         version;
        QString         systemInfo;
        QString         authKey;
    };
#define ApiMediaServerData_Fields ApiResourceData_Fields (networkAddresses)(flags)(version)(systemInfo)(authKey)

    QN_FUSION_DECLARE_FUNCTIONS(ApiMediaServerData, (eq))


    struct ApiMediaServerUserAttributesData: ApiData
    {
        QnUuid serverId;
        QString serverName;
        int maxCameras;
        bool allowAutoRedundancy; //< Server can take cameras from offline server automatically.

        // Redundant storage settings.
        Qn::BackupType backupType;
        int backupDaysOfTheWeek; //< Days of the week mask. See backup::DayOfWeek enum.
        int backupStart; //< Seconds from 00:00:00. Error if bDOW set and this is not set.
        int backupDuration; //< Duration of synchronization period in seconds. -1 if not set.
        int backupBitrate; //< Bitrate cap in bytes per second. Negative value if not capped. Not capped by default.

        ApiMediaServerUserAttributesData();
        QnUuid getIdForMerging() const { return serverId; } //< See ApiIdData::getIdForMerging().

        static DeprecatedFieldNames* getDeprecatedFieldNames()
        {
            static DeprecatedFieldNames kDeprecatedFieldNames{
                {lit("serverId"), lit("serverID")}, //< up to v2.6
            };
            return &kDeprecatedFieldNames;
        }
    };

#define ApiMediaServerUserAttributesData_Fields_Short   \
    (maxCameras)                                        \
    (allowAutoRedundancy)                               \
    (backupType)                                        \
    (backupDaysOfTheWeek)                               \
    (backupStart)                                       \
    (backupDuration)                                    \
    (backupBitrate)                                     \

#define ApiMediaServerUserAttributesData_Fields     \
    (serverId)                                      \
    (serverName)                                    \
    ApiMediaServerUserAttributesData_Fields_Short


    QN_FUSION_DECLARE_FUNCTIONS(ApiMediaServerUserAttributesData, (eq))

    struct ApiMediaServerDataEx
    :
        ApiMediaServerData,
        ApiMediaServerUserAttributesData
    {
        ApiMediaServerDataEx(): ApiMediaServerData(), ApiMediaServerUserAttributesData(), status(Qn::Offline) {}

        Qn::ResourceStatus status;
        std::vector<ApiResourceParamData> addParams;
        ApiStorageDataList storages;

        ApiMediaServerDataEx( const ApiMediaServerDataEx& mediaServerData )
        :
            ApiMediaServerData(mediaServerData),
            status( Qn::Offline )
        {
        }

        ApiMediaServerDataEx( ApiMediaServerData&& mediaServerData )
        :
            ApiMediaServerData( std::move( mediaServerData ) ),
            status( Qn::Offline )
        {
        }

        ApiMediaServerDataEx( ApiMediaServerDataEx&& mediaServerData )
        :
            ApiMediaServerData( std::move( mediaServerData ) ),
            ApiMediaServerUserAttributesData( std::move( mediaServerData ) ),
            status( mediaServerData.status ),
            addParams( std::move( mediaServerData.addParams ) ),
            storages( std::move( mediaServerData.storages ) )
        {
        }

        ApiMediaServerDataEx& operator=( ApiMediaServerDataEx&& mediaServerData )
        {
            static_cast< ApiMediaServerData& >( *this ) =
                    std::move( static_cast< ApiMediaServerData&& >( mediaServerData ) );
            static_cast< ApiMediaServerUserAttributesData& >( *this ) =
                    std::move( static_cast< ApiMediaServerUserAttributesData&& >( mediaServerData ) );

            status = mediaServerData.status;
            addParams = std::move( mediaServerData.addParams );
            storages = std::move( mediaServerData.storages );
            return *this;
        }

        ApiMediaServerDataEx& operator=(const ApiMediaServerDataEx&) = default;
    };
#define ApiMediaServerDataEx_Fields ApiMediaServerData_Fields ApiMediaServerUserAttributesData_Fields_Short (status)(addParams) (storages)

} // namespace ec2

#endif //MSERVER_DATA_H
