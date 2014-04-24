#ifndef __RESOURCE_TRANSACTION_DATA_H__
#define __RESOURCE_TRANSACTION_DATA_H__

#include "api_globals.h"
#include "api_data.h"


namespace ec2
{
    struct ApiResourceParamData: ApiData
    {
        ApiResourceParamData() {}
        ApiResourceParamData(const QString& name, const QString& value, bool isResTypeParam): value(value), name(name), isResTypeParam(isResTypeParam) {}

        QString value;
        QString name;
        bool isResTypeParam;

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiResourceParamData_Fields (value)(name)(isResTypeParam)


    struct ApiResourceParamWithRefData: ApiResourceParamData
    {
        QnId resourceId;
    };
#define ApiResourceParamWithRefData_Fields ApiResourceParamData_Fields (resourceId)


    struct ApiResourceParamsData: ApiData
    {
        QnId id;
        std::vector<ApiResourceParamData> params;
    };
#define ApiResourceParamsData_Fields (id)(params)


    struct ApiResourceData: ApiData {
        ApiResourceData(): status(QnResource::Offline), disabled(false) {}

        QnId          id;
        QnId          parentGuid;
        QnResource::Status    status;
        bool          disabled;
        QString       name;
        QString       url;
        QnId          typeId;
        std::vector<ApiResourceParamData> addParams;

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiResourceData_Fields (id)(parentGuid)(status)(disabled)(name)(url)(typeId)(addParams)


    struct ApiSetResourceDisabledData: ApiData {
        QnId id;
        bool disabled;
    };
#define ApiSetResourceDisabledData_Fields (id)(disabled)


    struct ApiSetResourceStatusData: ApiData
    {
        QnId id;
        QnResource::Status status;

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiSetResourceStatusData_Fields (id)(status)

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiResourceParamData, ApiResourceParamFields);

    /*struct ApiParamList
    {
        std::vector<ApiResourceParamData> data;
        void toResourceList(QnKvPairList& resources) const;
        void fromResourceList(const QnKvPairList& resources);
    };*/

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiParamList, (data) )
    //QN_FUSION_DECLARE_FUNCTIONS(ApiParamList, (binary))
        

    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiResourceParams,  (id) (params) )
    //QN_FUSION_DECLARE_FUNCTIONS(ApiResourceParams, (binary))

    /*void fromResourceToApi(const QnResourcePtr& resource, ApiResourceData& resourceData);
    void fromApiToResource(const ApiResourceData& resourceData, QnResourcePtr resource);

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiResourceData,  ApiResourceFields)

    struct ApiResourceList: public ApiData {
        void loadFromQuery(QSqlQuery& query);
        void toResourceList( QnResourceFactory* resFactory, QnResourceList& resList ) const;

        std::vector<ApiResourceData> data;
    };

    //QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS (ApiResourceList,  (data))
    QN_FUSION_DECLARE_FUNCTIONS(ApiResourceList, (binary))

    //QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ApiSetResourceStatusData,  (id) (status) )
    QN_FUSION_DECLARE_FUNCTIONS(ApiSetResourceStatusData, (binary))



    //QN_DEFINE_STRUCT_SERIALIZATORS (ApiSetResourceDisabledData,  (id) (disabled) )
    QN_FUSION_DECLARE_FUNCTIONS(ApiSetResourceDisabledData, (binary))*/

}

#endif // __RESOURCE_TRANSACTION_DATA_H__
