#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "core/resource/resource_type.h"
#include "api_data.h"

namespace ec2 
{
    struct ApiPropertyType;
    struct ApiResourceType;

    #include "ec2_resource_type_data_i.h"

    struct ApiPropertyType: ApiPropertyTypeData
    {
        void toResource(QnParamTypePtr resource) const;
    };

    struct ApiResourceType: ApiResourceTypeData
    {
	    void fromResource(const QnResourceTypePtr& resource);
	    void toResource(QnResourceTypePtr resource) const;
	    QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiResourceType, ApiResourceTypeFields);

    struct ApiResourceTypeList: ApiResourceTypeListData {
	    void loadFromQuery(QSqlQuery& query);
	    void toResourceTypeList(QnResourceTypeList& resTypeList) const;
    };

}

#endif // __EC2_RESOURCE_TYPE_DATA_H_
