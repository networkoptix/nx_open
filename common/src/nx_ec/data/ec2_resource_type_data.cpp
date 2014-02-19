#include "ec2_resource_type_data.h"

namespace ec2
{

void ApiPropertyType::toResource(QnParamTypePtr resource) const
{
    //resource->id = id;
    resource->name = name;
    resource->type = (QnParamType::DataType) type;
    resource->min_val = min;
    resource->max_val = max;
    resource->step = step;
    foreach(const QString& val, values.split(QLatin1Char(',')))
        resource->possible_values << val.trimmed();
    foreach(const QString& val, ui_values.split(QLatin1Char(',')))
        resource->ui_possible_values << val.trimmed();
    resource->default_value = default_value;
    resource->group = group;
    resource->subgroup = sub_group;
    resource->description = description;
    resource->ui = ui;
    resource->isReadOnly = readonly;
    resource->paramNetHelper = netHelper;
}


void ApiResourceTypeData::toResource(QnResourceTypePtr resource) const
{
	resource->setId(id);
	resource->setName(name);
	resource->setManufacture(manufacture);
	
	if (!parentId.empty())
		resource->setParentId(parentId[0]);
	for (int i = 1; i < parentId.size(); ++i)
		resource->addAdditionalParent(parentId[0]);
    foreach(const ApiPropertyType& p, propertyTypeList) {
        QnParamTypePtr param(new QnParamType());
        p.toResource(param);
        resource->addParamType(param);
    }
}

void ApiResourceTypeList::loadFromQuery(QSqlQuery& query)
{
	QN_QUERY_TO_DATA_OBJECT(query, ApiResourceTypeData, data, (id) (name) (manufacture) );
}

void ApiResourceTypeList::toResourceTypeList(QnResourceTypeList& resTypeList) const
{
	resTypeList.reserve(data.size());
	for(int i = 0; i < data.size(); ++i) {
		QnResourceTypePtr resType(new QnResourceType());
		data[i].toResource(resType);
		resTypeList << resType;
	}
}

}
