#ifndef QN_RESOURCE_TYPE_DATA_I_H
#define QN_RESOURCE_TYPE_DATA_I_H

#include "api_data_i.h"

namespace ec2 {
    struct ApiPropertyTypeData: ApiData {
        QnId resource_type_id;

        QString name;
        int type;

        // MinMaxStep
        qint32 min;
        qint32 max;
        qint32 step;

        // Enumaration
        QString values;
        QString ui_values;

        // Value
        QString default_value;

        QString group;
        QString sub_group;
        QString description;

        bool ui;
        bool readonly;

        QString netHelper;
    };

#define ApiPropertyTypeData_Fields (resource_type_id)(name)(type)(min)(max)(step)(values)(ui_values)(default_value)(group)(sub_group)(description)(ui)(readonly)(netHelper)


    struct ApiResourceTypeData: ApiData {
        QnId id;
        QString name;
        QString manufacture;
        std::vector<QnId> parentId;
        std::vector<ApiPropertyTypeData> propertyTypeList;

	    QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiResourceTypeData_Fields (id)(name)(manufacture)(parentId)(propertyTypeList)

} // namespace ec2

#endif // QN_RESOURCE_TYPE_DATA_I_H
