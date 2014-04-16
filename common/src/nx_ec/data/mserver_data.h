
#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "ec2_resource_data.h"
#include "nx_ec/ec_api.h"


namespace ec2
{
    struct ApiStorageData: public ApiResourceData
    {
        ApiStorageData(): spaceLimit(0), usedForWriting(0) {}

        qint64       spaceLimit;
        bool         usedForWriting;

        void fromResource(QnAbstractStorageResourcePtr resource);
        void toResource(QnAbstractStorageResourcePtr resource) const;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiStorageDataFields  (spaceLimit) (usedForWriting)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS (ApiStorageData, ec2::ApiResourceData, ApiStorageDataFields)


    struct ApiStorageDataList: public ApiData {
        std::vector<ApiStorageData> data;

        void loadFromQuery(QSqlQuery& query);
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiStorageDataList, (data))


    struct ApiMediaServerData: public ApiResourceData
    {
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(0) {}

        QString      apiUrl;
        QString      netAddrList;
        Qn::ServerFlags flags;
        qint32       panicMode;
        QString      streamingUrl;
        QString      version; 
        QString      systemInfo;
        QString      authKey;
        std::vector<ApiStorageData>  storages;
        
        void fromResource(QnMediaServerResourcePtr resource);
        void toResource(QnMediaServerResourcePtr resource, const ResourceContext& ctx) const;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define medisServerDataFields (apiUrl) (netAddrList) (flags) (panicMode) (streamingUrl) (version) (systemInfo) (authKey) (storages)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS (ApiMediaServerData, ec2::ApiResourceData, medisServerDataFields)


    struct ApiPanicModeData: public ApiData
    {
        Qn::PanicMode mode;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiPanicModeData, (mode))


    struct ApiMediaServerDataList: public ApiData {
        std::vector<ApiMediaServerData> data;

        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData, const ResourceContext& ctx) const;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiMediaServerDataList, (data))
}

#endif //MSERVER_DATA_H
