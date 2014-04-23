#ifndef QN_MSERVER_DATA_I_H
#define QN_MSERVER_DATA_I_H

#include "api_types_i.h"
#include "ec2_resource_data_i.h"

namespace ec2 {

    struct ApiStorageData: ApiResourceData
    {
        ApiStorageData(): spaceLimit(0), usedForWriting(0) {}

        qint64       spaceLimit;
        bool         usedForWriting;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiStorageData_Fields  (spaceLimit)(usedForWriting)


    struct ApiMediaServerData: ApiResourceData
    {
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(0), maxCameras(0), redundancy(false) {}

        QString      apiUrl;
        QString      netAddrList;
        Qn::ServerFlags flags;
        qint32       panicMode;
        QString      streamingUrl;
        QString      version; 
        QString      authKey;
        std::vector<ApiStorageData> storages;
        int maxCameras;
        bool redundancy;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiMediaServerData_Fields (apiUrl)(netAddrList)(flags)(panicMode)(streamingUrl)(version)(authKey)(storages)(maxCameras)(redundancy)
	
} // namespace ec2 


#endif // QN_MSERVER_DATA_I_H