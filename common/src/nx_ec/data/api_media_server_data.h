#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{
    namespace redundant 
    {
        enum DaysOfWeek
        {
            NoDay       = 0,
            Monday      = 2,
            Tuesday     = 4,
            Wednsday    = 8,
            Thursday    = 16,
            Friday      = 32,
            Saturday    = 64,
            Sunday      = 128
        };
    }

    struct ApiStorageData: ApiResourceData
    {
        ApiStorageData(): spaceLimit(0), usedForWriting(0) {}

        qint64          spaceLimit;
        bool            usedForWriting;
        QString         storageType;
        bool            redundant;              // is storage redundant
        int             redundantDaysOfTheWeek; // Days of the week mask. 0 if not set
        int             redundantStart;         // seconds from 00:00.
        int             redundantDuration;      // duration of synchronization period

        std::vector<ApiResourceParamData> addParams;
    };
#define ApiStorageData_Fields   \
    ApiResourceData_Fields      \
    (spaceLimit)                \
    (usedForWriting)            \
    (storageType)               \
    (redundant)                 \
    (redundantDaysOfTheWeek)    \
    (redundantStart)            \
    (redundantDuration)         \
    (addParams)


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
        ApiMediaServerUserAttributesData(): maxCameras(0), allowAutoRedundancy(false) {}

        QnUuid          serverID;
        QString         serverName;
        int             maxCameras;
        bool            allowAutoRedundancy; // Server can take cameras from offline server automatically
    };

#define ApiMediaServerUserAttributesData_Fields_Short (maxCameras)(allowAutoRedundancy)
#define ApiMediaServerUserAttributesData_Fields (serverID) (serverName) ApiMediaServerUserAttributesData_Fields_Short


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
