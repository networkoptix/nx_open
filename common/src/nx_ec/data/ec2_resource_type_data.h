#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "core/resource/resource_type.h"
#include "api_data.h"

namespace ec2 
{
    #include "ec2_resource_type_data_i.h"

    void fromApiToResource(const ApiPropertyTypeData &data, QnParamTypePtr& resource);

	void fromResourceToApi(const QnResourceTypePtr& resource, ApiResourceTypeData &data);
	void fromApiToResource(const ApiResourceTypeData &data, QnResourceTypePtr resource);

    QN_DEFINE_STRUCT_SQL_BINDER(ApiResourceTypeData, ApiResourceTypeFields);

	void loadResourceTypesFromQuery(ApiResourceTypeDataListData &data, QSqlQuery& query);
	void fromApiToResourceTypeList(const ApiResourceTypeDataListData &data, QnResourceTypeList& resTypeList);
}

#endif // __EC2_RESOURCE_TYPE_DATA_H_
