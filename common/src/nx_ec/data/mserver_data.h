#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "ec2_resource_data.h"
#include "nx_ec/ec_api.h"


namespace ec2
{
    struct ApiStorage;
    typedef std::vector<ApiStorage> StorageVector;

    #include "mserver_data_i.h"

    struct ApiStorage: ApiStorageData, ApiResource
    {
        void fromResource(QnAbstractStorageResourcePtr resource);
        void toResource(QnAbstractStorageResourcePtr resource) const;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };
    QN_DEFINE_STRUCT_SQL_BINDER(ApiStorage, ApiStorageFields);



    struct ApiStorageList: public ApiData {
        std::vector<ApiStorage> data;

        void loadFromQuery(QSqlQuery& query);
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiStorageList, (data))

    struct ApiMediaServer: ApiMediaServerData, ApiResource
    {
        void fromResource(QnMediaServerResourcePtr resource);
        void toResource(QnMediaServerResourcePtr resource, const ResourceContext& ctx) const;
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiMediaServer, medisServerDataFields);

    struct ApiPanicModeData: public ApiData
    {
        Qn::PanicMode mode;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiPanicModeData, (mode))


    struct ApiMediaServerList: public ApiData {
        std::vector<ApiMediaServer> data;

        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData, const ResourceContext& ctx) const;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiMediaServerList, (data))
}

#endif //MSERVER_DATA_H
