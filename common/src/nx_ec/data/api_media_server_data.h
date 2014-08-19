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
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(Qn::PM_None), maxCameras(0), allowAutoRedundancy(false) {}

        QString         apiUrl;
        QString         networkAddresses;
        Qn::ServerFlags flags;
        Qn::PanicMode   panicMode;
        QString         version; 
        QString         systemInfo;
        QString         authKey;
        std::vector<ApiStorageData> storages;
        int             maxCameras;
        bool            allowAutoRedundancy; // Server can take cameras from offline server automatically
        QString         systemName; //! < Server system name. It can be invalid sometimes, but it matters only when server is in incompatible state.
    };
#define ApiMediaServerData_Fields ApiResourceData_Fields (apiUrl)(networkAddresses)(flags)(panicMode)(version)(systemInfo)(authKey)(storages)(maxCameras)(allowAutoRedundancy)(systemName)


    struct ApiPanicModeData: public ApiData
    {
        Qn::PanicMode mode;
    };
#define ApiPanicModeData_Fields (mode)

} // namespace ec2

#endif //MSERVER_DATA_H
