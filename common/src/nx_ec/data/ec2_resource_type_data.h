#ifndef __EC2_RESOURCE_TYPE_DATA_H_
#define __EC2_RESOURCE_TYPE_DATA_H_

#include "core/resource/resource_type.h"
#include "api_data.h"

namespace ec2 
{

struct ApiResourceTypeData: public ApiData
{
	qint32 id;
	QString name;
	QString manufacture;
	std::vector<qint32> parentId;

	void fromResource(const QnResourceTypePtr& resource);
	void toResource(QnResourceTypePtr resource);
	QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS();
};

struct ApiResourceTypeList: public ApiData {
	std::vector<ApiResourceTypeData> data;

	void loadFromQuery(QSqlQuery& query);
	QN_DECLARE_STRUCT_SERIALIZATORS();
};

}

QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiResourceTypeData, (id) (name) (manufacture) (parentId) )
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiResourceTypeList, (resourceTypes))


#endif // __EC2_RESOURCE_TYPE_DATA_H_
