#include "ec2_resource_data.h"

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

        QnResourceParameters toHashMap() const;
        void fromResource(QnAbstractStorageResourcePtr resource);
        void toResource(QnAbstractStorageResourcePtr resource) const;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    struct ApiStorageDataList: public ApiData {
        std::vector<ApiStorageData> data;

        void loadFromQuery(QSqlQuery& query);
    };

    struct ApiMediaServerData: public ApiResourceData
    {
        ApiMediaServerData(): flags(Qn::SF_None), panicMode(0) {}

        QString      apiUrl;
        QString      netAddrList;
        Qn::ServerFlags flags;
        qint32       panicMode;
        QString      streamingUrl;
        QString      version; 
        QString      authKey;
        std::vector<ApiStorageData>  storages;
        
        void fromResource(QnMediaServerResourcePtr resource);
        void toResource(QnMediaServerResourcePtr resource, const ResourceContext& ctx) const;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    struct ApiPanicModeData: public ApiData
    {
        Qn::PanicMode mode;
    };

    struct ApiMediaServerDataList: public ApiData {
        std::vector<ApiMediaServerData> data;

        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData, const ResourceContext& ctx) const;
    };
}

#define ApiStorageDataFields  (spaceLimit) (usedForWriting)
#define medisServerDataFields (apiUrl) (netAddrList) (flags) (panicMode) (streamingUrl) (version) (authKey) (storages)

QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiStorageData, ApiStorageDataFields)
QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiMediaServerData, ec2::ApiResourceData, medisServerDataFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiMediaServerDataList, (data))
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiStorageDataList, (data))
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiPanicModeData, (mode))

#endif //MSERVER_DATA_H
