#include "ec2_resource_data.h"

#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "ec2_resource_data.h"


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
        QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS();
    };

    struct ApiStorageDataList: public ApiData {
        std::vector<ApiStorageData> data;

        void loadFromQuery(QSqlQuery& query);
        QN_DECLARE_STRUCT_SERIALIZATORS();
    };

    struct ApiMediaServerData: public ApiResourceData
    {
        ApiMediaServerData(): reserve(false), panicMode(0) {}

        QString      apiUrl;
        QString      netAddrList;
        bool         reserve;
        qint32       panicMode;
        QString      streamingUrl;
        QString      version; 
        QString      authKey;
        std::vector<ApiStorageData>  storages;
        
        void fromResource(QnMediaServerResourcePtr resource);
        void toResource(QnMediaServerResourcePtr resource, QnResourceFactory* factory, const QnResourceTypePool* resTypePool) const;
        QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS();
    };

    struct ApiMediaServerDataList: public ApiData {
        std::vector<ApiMediaServerData> data;

        void loadFromQuery(QSqlQuery& query);
        void toResourceList(QnMediaServerResourceList& outData, QnResourceFactory* factory, const QnResourceTypePool* resTypePool) const;
        QN_DECLARE_STRUCT_SERIALIZATORS();
    };
}

#define ApiStorageDataFields  (spaceLimit) (usedForWriting)
#define medisServerDataFields (apiUrl) (netAddrList) (reserve) (panicMode) (streamingUrl) (version) (authKey) (storages)

QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiStorageData, ApiStorageDataFields)
QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiMediaServerData, ApiResourceData, medisServerDataFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiMediaServerDataList, (data))
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiStorageDataList, (data))

#endif //MSERVER_DATA_H
