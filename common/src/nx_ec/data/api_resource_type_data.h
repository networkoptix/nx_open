#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2 
{
    struct ApiPropertyTypeData: ApiData {
        QnUuid resourceTypeId;

        QString name;
        QString defaultValue;
    };
#define ApiPropertyTypeData_Fields (resourceTypeId)(name)(defaultValue)


    struct ApiResourceTypeData: ApiData {
        QnUuid id;
        QString name;
        QString vendor;
        std::vector<QnUuid> parentId;
        std::vector<ApiPropertyTypeData> propertyTypes;
    };
#define ApiResourceTypeData_Fields (id)(name)(vendor)(parentId)(propertyTypes)

}

#endif // __EC2_RESOURCE_TYPE_DATA_H_
