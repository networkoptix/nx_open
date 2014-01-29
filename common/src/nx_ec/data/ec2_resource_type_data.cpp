#include "ec2_resource_type_data.h"

namespace ec2
{
void ApiResourceTypeList::loadFromQuery(QSqlQuery& query)
{
	QN_QUERY_TO_DATA_OBJECT(ApiResourceTypeData, data, (id) (name) (manufacture) );
}

}
