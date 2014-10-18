#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{

    struct ApiStorageData: ApiResourceData
    {
        ApiStorageData(): spaceLimit(0), usedForWriting(0) {}

        qint64          spaceLimit;
        bool            usedForWriting;
    };
#define ApiStorageData_Fields ApiResourceData_Fields (spaceLimit)(usedForWriting)


    struct ApiMediaServerData: ApiResourceData
    {
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(Qn::PM_None) {}

        QString         apiUrl;
        QString         networkAddresses;
        Qn::ServerFlags flags;
        Qn::PanicMode   panicMode;
        QString         version; 
        QString         systemInfo;
        QString         authKey;
        QString         systemName; //! < Server system name. It can be invalid sometimes, but it matters only when server is in incompatible state.
    };
#define ApiMediaServerData_Fields ApiResourceData_Fields (apiUrl)(networkAddresses)(flags)(panicMode)(version)(systemInfo)(authKey)(systemName)


    struct ApiMediaServerUserAttributesData: ApiData
    {
        ApiMediaServerUserAttributesData(): maxCameras(0), allowAutoRedundancy(false) {}

        QnUuid          serverID;
        QString         serverName;
        int             maxCameras;
        bool            allowAutoRedundancy; // Server can take cameras from offline server automatically
    };

#define ApiMediaServerUserAttributesData_Fields_Short (serverName)(maxCameras)(allowAutoRedundancy)
#define ApiMediaServerUserAttributesData_Fields (serverID) ApiMediaServerUserAttributesData_Fields_Short


    struct ApiMediaServerDataEx
    :
        ApiMediaServerData,
        ApiMediaServerUserAttributesData
    {
        Qn::ResourceStatus status;
        std::vector<ApiResourceParamData> addParams;
        ApiStorageDataList storages;

        ApiMediaServerDataEx();

        template<class ApiMediaServerDataRefType>
        ApiMediaServerDataEx( ApiMediaServerDataRefType&& mediaServerData )
        :
            ApiMediaServerData( std::forward<ApiMediaServerDataRefType>(mediaServerData) )
        {
        }
    };
#define ApiMediaServerDataEx_Fields ApiMediaServerData_Fields ApiMediaServerUserAttributesData_Fields_Short (status)(addParams) (storages)


    struct ApiPanicModeData: public ApiData
    {
        Qn::PanicMode mode;
    };
#define ApiPanicModeData_Fields (mode)

} // namespace ec2

#endif //MSERVER_DATA_H
