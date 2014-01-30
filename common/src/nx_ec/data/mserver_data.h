#include "ec2_resource_data.h"

#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "ec2_resource_data.h"


namespace ec2
{
    struct ApiStorageData: public ApiData
    {
        qint32       id;
        QString      name;
        QString      url;
        qint64       spaceLimit;
        bool         usedForWriting;

        QnResourceParameters toHashMap(const QString& serverId) const;
        void toResource(QnAbstractStorageResourcePtr resource) const;
        QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS();
    };

    struct ApiMediaServerData: public ApiResourceData
    {
        QString      apiUrl;
        QString      netAddrList;
        bool         reserve;
        qint32       panicMode;
        QString      streamingUrl;
        QString      version; 
        QString      authKey;
        std::vector<ApiStorageData>  storages;
        
        void toResource(QnMediaServerResourcePtr resource, QnResourceFactory* factory) const;
        QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS();
    };

    struct ApiMediaServerDataList: public ApiData {
        std::vector<ApiMediaServerData> data;

        void loadFromQuery(QSqlQuery& query);
        void toResourceList(QnMediaServerResourceList& outData, QnResourceFactory* factory) const;
        QN_DECLARE_STRUCT_SERIALIZATORS();
    };
}

#define ApiStorageDataFields (id) (name) (url) (spaceLimit) (usedForWriting)
#define medisServerDataFields (apiUrl) (netAddrList) (reserve) (storages) (panicMode) (streamingUrl) (version)

QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiStorageData, ApiStorageDataFields)
QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiMediaServerData, ApiResourceData, medisServerDataFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiMediaServerDataList, (data))

#endif //MSERVER_DATA_H
