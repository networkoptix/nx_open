#ifndef _eve_parse_users_
#define _eve_parse_users_

#include "api/Types.h"
#include "core/resource/resource.h"

void parseUsers(QList<QnResourcePtr>& users, const QnApiUsers& xsdUsers);

#endif // _eve_parse_users_
