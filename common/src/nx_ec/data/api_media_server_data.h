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
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(0), maxCameras(0), redundancy(false) {}

        QString         apiUrl;
        QString         netAddrList; // TODO: #API rename 'networkAddresses'. We don't use -List suffix in naming, we use plurals.
        Qn::ServerFlags flags;
        qint32          panicMode; // TODO: #API use Qn::PanicMode
        QString         streamingUrl;
        QString         version; 
        QString         systemInfo;
        QString         authKey;
        std::vector<ApiStorageData> storages;
        int             maxCameras;
        bool            redundancy; // TODO: #API what is this? Naming is unclear and I don't know how to rename it. Add comments to QnMediaServerResource::isRedundancy at least.
    };
#define ApiMediaServerData_Fields ApiResourceData_Fields (apiUrl)(netAddrList)(flags)(panicMode)(streamingUrl)(version)(systemInfo)(authKey)(storages)(maxCameras)(redundancy)


    struct ApiPanicModeData: public ApiData
    {
        Qn::PanicMode mode;
    };
#define ApiPanicModeData_Fields (mode)

} // namespace ec2

#endif //MSERVER_DATA_H
