#include "ec2_resource_type_data.h"

namespace ec2
{

void fromApiToResource(const ApiPropertyTypeData& data, QnParamTypePtr& resource)
{
    //resource->id = id;
    resource->name = data.name;
    resource->type = (QnParamType::DataType) data.type;
    resource->min_val = data.min;
    resource->max_val = data.max;
    resource->step = data.step;
    foreach(const QString& val, data.values.split(QLatin1Char(',')))
        resource->possible_values << val.trimmed();
    foreach(const QString& val, data.ui_values.split(QLatin1Char(',')))
        resource->ui_possible_values << val.trimmed();
    resource->default_value = data.default_value;
    resource->group = data.group;
    resource->subgroup = data.sub_group;
    resource->description = data.description;
    resource->ui = data.ui;
    resource->isReadOnly = data.readonly;
    resource->paramNetHelper = data.netHelper;
}


void fromApiToResource(const ApiResourceTypeData& data, QnResourceTypePtr resource)
{
	resource->setId(data.id);
	resource->setName(data.name);
	resource->setManufacture(data.manufacture);
	
	if (!data.parentId.empty())
		resource->setParentId(data.parentId[0]);
	for (int i = 1; i < data.parentId.size(); ++i)
		resource->addAdditionalParent(data.parentId[i]);
    foreach(const ApiPropertyTypeData& p, data.propertyTypeList) {
        QnParamTypePtr param(new QnParamType());
        fromApiToResource(p, param);
        resource->addParamType(param);
    }
}

void loadResourceTypesFromQuery(ApiResourceTypeDataListData &data, QSqlQuery& query)
{
	QN_QUERY_TO_DATA_OBJECT(query, ApiResourceTypeData, data.data, (id) (name) (manufacture) );
}

void fromApiToResourceTypeList(const ApiResourceTypeDataListData &data, QnResourceTypeList& resTypeList)
{
	resTypeList.reserve((int)data.data.size());
	for(int i = 0; i < data.data.size(); ++i) {
		QnResourceTypePtr resType(new QnResourceType());
		fromApiToResource(data.data[i], resType);
		resTypeList << resType;
	}
}

}
