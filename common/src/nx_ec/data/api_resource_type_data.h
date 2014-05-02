#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2 
{
    struct ApiPropertyTypeData: ApiData {
        QnId resource_type_id; // TODO: #API we don't use under_scores, we use camelCase.

        QString name;
        int type;

        // MinMaxStep
        qint32 min;
        qint32 max;
        qint32 step;

        // Enumaration
        QString values;
        QString ui_values; // TODO: #API camelCase

        // Value
        QString default_value; // TODO: #API camelCase

        QString group;
        QString sub_group; // TODO: #API camelCase
        QString description;

        bool ui;
        bool readonly; // TODO: #API camelCase => readOnly

        QString netHelper; // TODO: #API I have no idea what this is. What is this, actually?
    };
#define ApiPropertyTypeData_Fields (resource_type_id)(name)(type)(min)(max)(step)(values)(ui_values)(default_value)(group)(sub_group)(description)(ui)(readonly)(netHelper)


    struct ApiResourceTypeData: ApiData {
        QnId id;
        QString name;
        QString manufacture; // TODO: #API and now it's our bad naming leaking out. Rename 'vendor'.
        std::vector<QnId> parentId;
        std::vector<ApiPropertyTypeData> propertyTypeList; // TODO: #API We don't use -List suffix, we use plurals. Rename 'propertyTypes'.
    };
#define ApiResourceTypeData_Fields (id)(name)(manufacture)(parentId)(propertyTypeList)

}

#endif // __EC2_RESOURCE_TYPE_DATA_H_
