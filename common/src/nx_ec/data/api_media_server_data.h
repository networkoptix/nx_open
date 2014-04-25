#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{

    struct ApiStorageData: ApiResourceData
    {
        ApiStorageData(): spaceLimit(0), usedForWriting(0) {}

        qint64       spaceLimit;
        bool         usedForWriting;

        ///QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiStorageData_Fields ApiResourceData_Fields (spaceLimit)(usedForWriting)


    struct ApiMediaServerData: ApiResourceData
    {
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(0), maxCameras(0), redundancy(false) {}

        QString      apiUrl;
        QString      netAddrList;
        Qn::ServerFlags flags;
        qint32       panicMode; // TODO: #Elric #EC2 use Qn::PanicMode
        QString      streamingUrl;
        QString      version; 
        QString      authKey;
        std::vector<ApiStorageData> storages;
        int maxCameras;
        bool redundancy;

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiMediaServerData_Fields ApiResourceData_Fields (apiUrl)(netAddrList)(flags)(panicMode)(streamingUrl)(version)(authKey)(storages)(maxCameras)(redundancy)


    struct ApiPanicModeData: public ApiData
    {
        Qn::PanicMode mode;
    };
#define ApiPanicModeData_Fields (mode)


    /*void fromResourceToApi(const QnAbstractStorageResourcePtr &resource, ApiStorageData &data);
    void fromApiToResource(const ApiStorageData &data, QnAbstractStorageResourcePtr &resource);

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiStorageData, ApiStorageFields);

    /*struct ApiStorageList: public ApiData {
        std::vector<ApiStorageData> data;

        void loadFromQuery(QSqlQuery& query);
    };*/

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiStorageList, (data))
    //QN_FUSION_DECLARE_FUNCTIONS(ApiStorageList, (binary))

    /*void fromResourceToApi(const QnMediaServerResourcePtr& resource, ApiMediaServerData &data);
    void fromApiToResource(const ApiMediaServerData &data, QnMediaServerResourcePtr& resource, const ResourceContext& ctx);

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiMediaServerData, medisServerDataFields);

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiPanicModeData, (mode))
    QN_FUSION_DECLARE_FUNCTIONS(ApiPanicModeData, (binary))


    struct ApiMediaServerList: public std::vector<ApiMediaServerData> {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData, const ResourceContext& ctx) const;
    };*/
}

#endif //MSERVER_DATA_H
