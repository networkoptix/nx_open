#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "core/resource/resource_type.h"
#include "api_data.h"

namespace ec2 
{

struct ApiPropertyType: public ApiData
{
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

    void toResource(QnParamTypePtr resource) const;
};

struct ApiResourceTypeData: public ApiData
{
	QnId id;
	QString name;
	QString manufacture;
	std::vector<QnId> parentId;
    std::vector<ApiPropertyType> propertyTypeList;

	void fromResource(const QnResourceTypePtr& resource);
	void toResource(QnResourceTypePtr resource) const;
	QN_DECLARE_STRUCT_SQL_BINDER();
};

struct ApiResourceTypeList: public ApiData {
	void loadFromQuery(QSqlQuery& query);
	void toResourceTypeList(QnResourceTypeList& resTypeList) const;

	std::vector<ApiResourceTypeData> data;
};

}

#define ApiPropertyTypeFields (resource_type_id) (name) (type) (min) (max) (step) (values) (ui_values) (default_value) (group) (sub_group) (description) (ui) (readonly) (netHelper)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiPropertyType, ApiPropertyTypeFields)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiResourceTypeData, (id) (name) (manufacture) (parentId) (propertyTypeList) )
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiResourceTypeList, (data))


#endif // __EC2_RESOURCE_TYPE_DATA_H_
