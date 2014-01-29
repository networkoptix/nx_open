#include "ec2_resource_type_data.h"

namespace ec2
{

void ApiResourceTypeData::toResource(QnResourceTypePtr resource) const
{
	resource->setId(id);
	resource->setName(name);
	resource->setManufacture(manufacture);
	
	if (!parentId.empty())
		resource->setParentId(parentId[0]);
	for (int i = 1; i < parentId.size(); ++i)
		resource->addAdditionalParent(parentId[0]);
}

void ApiResourceTypeList::loadFromQuery(QSqlQuery& query)
{
	QN_QUERY_TO_DATA_OBJECT(ApiResourceTypeData, data, (id) (name) (manufacture) );
}

void ApiResourceTypeList::toResourceTypeList(QnResourceTypeList resTypeList) const
{
	resTypeList.reserve(data.size());
	for(int i = 0; i < data.size(); ++i) {
		QnResourceTypePtr resType(new QnResourceType());
		data[i].toResource(resType);
		resTypeList << resType;
	}
}

}
