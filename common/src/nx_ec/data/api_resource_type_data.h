#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiPropertyTypeData: nx::vms::api::Data
    {
        QnUuid resourceTypeId;

        QString name;
        QString defaultValue;
    };
#define ApiPropertyTypeData_Fields (resourceTypeId)(name)(defaultValue)


    struct ApiResourceTypeData: ApiIdData {
        QString name;
        QString vendor;
        std::vector<QnUuid> parentId;
        std::vector<ApiPropertyTypeData> propertyTypes;
    };
#define ApiResourceTypeData_Fields IdData_Fields (name)(vendor)(parentId)(propertyTypes)

}

#endif // __EC2_RESOURCE_TYPE_DATA_H_
