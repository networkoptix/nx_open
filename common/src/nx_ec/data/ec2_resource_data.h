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
    
struct ApiResourceParam: public ApiData
{
    ApiResourceParam() {}
    ApiResourceParam(const QString& name, const QString& value): name(name), value(value) {}

    QString name;
    QString value;

    QN_DECLARE_STRUCT_SQL_BINDER();
};

struct ApiResourceParamWithRef: public ApiResourceParam
{
    QnId resourceId;
};

struct ApiResourceParams
{
    QnId id;
    std::vector<ApiResourceParam> params;
};

struct ApiResourceData: public ApiData 
{
    ApiResourceData(): status(QnResource::Offline), disabled(false) {}

    QnId          id;
    QnId          typeId;
    QnId          parentGuid;
    QString       name;
    QString       url;
    QnResource::Status    status;
    bool          disabled;
    std::vector<ApiResourceParam> addParams;

	void fromResource(const QnResourcePtr& resource);
	void toResource(QnResourcePtr resource) const;
    QN_DECLARE_STRUCT_SQL_BINDER();
};

struct ApiResourceDataList: public ApiData {
	void loadFromQuery(QSqlQuery& query);
	void toResourceList( QnResourceFactory* resFactory, QnResourceList& resList ) const;

	std::vector<ApiResourceData> data;
};

struct ApiSetResourceStatusData: public ApiData
{
    QnId id;
    QnResource::Status    status;

    QN_DECLARE_STRUCT_SQL_BINDER();
};

typedef std::vector<ApiSetResourceStatusData> ApiSyncResponseData;

struct ApiSetResourceDisabledData: public ApiData {
    QnId id;
    bool disabled;
};

}

#define ApiResourceParamFields (name) (value)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiResourceParam, ApiResourceParamFields)

#define ApiResourceDataFields (id) (typeId) (parentGuid) (name) (url) (status) (disabled) (addParams)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiResourceData,  ApiResourceDataFields)
QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS (ec2::ApiResourceDataList,  (data))
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiSetResourceStatusData,  (id) (status) )
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiSetResourceDisabledData,  (id) (disabled) )
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiResourceParams,  (id) (params) )

#endif // __RESOURCE_TRANSACTION_DATA_H__
