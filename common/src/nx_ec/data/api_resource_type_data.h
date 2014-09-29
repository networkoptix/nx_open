#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2 
{
    struct ApiPropertyTypeData: ApiData {
        QnUuid resourceTypeId;

        QString name;
        Qn::PropertyDataType type;

        // MinMaxStep
        qint32 min;
        qint32 max;
        qint32 step;

        // Enumaration
        QString values;
        QString uiValues;

        // Value
        QString defaultValue;

        QString group;
        QString subGroup;
        QString description;

        bool ui;
        bool readOnly;

        QString internalData; // additional parameter data used for internal software purpose
    };
#define ApiPropertyTypeData_Fields (resourceTypeId)(name)(type)(min)(max)(step)(values)(uiValues)(defaultValue)(group)(subGroup)(description)(ui)(readOnly)(internalData)


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
