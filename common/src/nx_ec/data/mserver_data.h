#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "ec2_resource_data.h"
#include "nx_ec/ec_api.h"
#include "mserver_data_i.h"


namespace ec2
{

    void fromResourceToApi(const QnAbstractStorageResourcePtr &resource, ApiStorageData &data);
    void fromApiToResource(const ApiStorageData &data, QnAbstractStorageResourcePtr &resource);

    QN_DEFINE_STRUCT_SQL_BINDER(ApiStorageData, ApiStorageFields);

    struct ApiStorageList: public ApiData {
        std::vector<ApiStorageData> data;

        void loadFromQuery(QSqlQuery& query);
    };

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiStorageList, (data))
    QN_FUSION_DECLARE_FUNCTIONS(ApiStorageList, (binary))

    void fromResourceToApi(const QnMediaServerResourcePtr& resource, ApiMediaServerData &data);
    void fromApiToResource(const ApiMediaServerData &data, QnMediaServerResourcePtr& resource, const ResourceContext& ctx);

    QN_DEFINE_STRUCT_SQL_BINDER(ApiMediaServerData, medisServerDataFields);

    struct ApiPanicModeData: public ApiData
    {
        Qn::PanicMode mode;
    };

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiPanicModeData, (mode))
    QN_FUSION_DECLARE_FUNCTIONS(ApiPanicModeData, (binary))


    struct ApiMediaServerList: public ApiMediaServerDataListData {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData, const ResourceContext& ctx) const;
    };
}

#endif //MSERVER_DATA_H
