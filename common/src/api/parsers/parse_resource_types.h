#ifndef _eve_parse_resource_types_
#define _eve_parse_resource_types_

#include <QList>


#include "api/Types.h"
#include "core/resource/resource_type.h"

void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const QnApiResourceTypes& xsdResourceTypes);

#endif // _eve_parse_resource_types_
