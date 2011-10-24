#ifndef _eve_parse_resource_types_
#define _eve_parse_resource_types_

#include <QList>

#include "device/resource_type.h"
#include "api/Types.h"

void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const QnApiResourceTypes& xsdResourceTypes);

#endif // _eve_parse_resource_types_
