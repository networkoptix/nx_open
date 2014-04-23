#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2 
{
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

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiResourceTypeData_Fields (id)(name)(manufacture)(parentId)(propertyTypeList)

    /*void fromApiToResource(const ApiPropertyTypeData &data, QnParamTypePtr& resource);

	void fromResourceToApi(const QnResourceTypePtr& resource, ApiResourceTypeData &data);
	void fromApiToResource(const ApiResourceTypeData &data, QnResourceTypePtr resource);*/

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiResourceTypeData, ApiResourceTypeFields);

	//void loadResourceTypesFromQuery(ApiResourceTypeDataListData &data, QSqlQuery& query);
	//void fromApiToResourceTypeList(const ApiResourceTypeDataListData &data, QnResourceTypeList& resTypeList);
}

#endif // __EC2_RESOURCE_TYPE_DATA_H_
