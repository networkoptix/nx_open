#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{
    namespace backup 
    {
        //TODO: #rvasilenko move out to more general space
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
        ApiStorageData(): spaceLimit(0), usedForWriting(0), isBackup(false) {}

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
        ApiMediaServerData(): flags(Qn::SF_None), not_used(Qn::PM_None) {}

        QString         apiUrl;
        QString         networkAddresses;
        Qn::ServerFlags flags;
        Qn::PanicMode   not_used;
        QString         version; 
        QString         systemInfo;
        QString         authKey;
        QString         systemName; //! < Server system name. It can be invalid sometimes, but it matters only when server is in incompatible state.
    };
#define ApiMediaServerData_Fields ApiResourceData_Fields (apiUrl)(networkAddresses)(flags)(not_used)(version)(systemInfo)(authKey)(systemName)

    QN_FUSION_DECLARE_FUNCTIONS(ApiMediaServerData, (eq))


    struct ApiMediaServerUserAttributesData: ApiData
    {
        ApiMediaServerUserAttributesData();

        QnUuid          serverID;
        QString         serverName;
        int             maxCameras;
        bool            allowAutoRedundancy; // Server can take cameras from offline server automatically

        // redundant storage settings
        Qn::BackupType      backupType;
        int                 backupDaysOfTheWeek; // Days of the week mask. See backup::DayOfWeek enum 
        int                 backupStart;         // Seconds from 00:00:00. Error if bDOW set and this is not set
        int                 backupDuration;      // Duration of synchronization period in seconds. -1 if not set.
        int                 backupBitrate;       // Bitrate cap in bytes per second. Negative value if not capped. 
                                                 // Not capped by default
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
    (serverID)                                      \
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
    };
#define ApiMediaServerDataEx_Fields ApiMediaServerData_Fields ApiMediaServerUserAttributesData_Fields_Short (status)(addParams) (storages)

} // namespace ec2

#endif //MSERVER_DATA_H
