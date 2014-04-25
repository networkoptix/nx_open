#ifndef __RESOURCE_TRANSACTION_DATA_H__
#define __RESOURCE_TRANSACTION_DATA_H__

#include "api_data.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/resource.h"

#include <QtCore/qglobal.h>
#include <QString>
#include <vector>
#include "utils/common/id.h"


namespace ec2
{
    #include "ec2_resource_data_i.h"

    QN_DEFINE_STRUCT_SQL_BINDER(ApiResourceParamData, ApiResourceParamFields);

    struct ApiParamList
    {
        std::vector<ApiResourceParamData> data;
        void toResourceList(QnKvPairList& resources) const;
        void fromResourceList(const QnKvPairList& resources);
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiParamList, (data) )
        

    struct ApiResourceParamWithRef: public ApiResourceParamData
    {
        QnId resourceId;
    };

    struct ApiResourceParams
    {
        QnId id;
        std::vector<ApiResourceParamData> params;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiResourceParams,  (id) (params) )

    void fromResourceToApi(const QnResourcePtr& resource, ApiResourceData& resourceData);
    void fromApiToResource(const ApiResourceData& resourceData, QnResourcePtr resource);

    QN_DEFINE_STRUCT_SQL_BINDER(ApiResourceData,  ApiResourceFields)

    struct ApiResourceList: public ApiData {
        void loadFromQuery(QSqlQuery& query);
        void toResourceList( QnResourceFactory* resFactory, QnResourceList& resList ) const;

        std::vector<ApiResourceData> data;
    };

    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS (ApiResourceList,  (data))

    struct ApiSetResourceStatusData: public ApiData
    {
        QnId id;
        QnResource::Status status;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ApiSetResourceStatusData,  (id) (status) )


    typedef std::vector<ApiSetResourceStatusData> ApiSyncResponseData;

    struct ApiSetResourceDisabledData: public ApiData {
        QnId id;
        bool disabled;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiSetResourceDisabledData,  (id) (disabled) )

}

#endif // __RESOURCE_TRANSACTION_DATA_H__
