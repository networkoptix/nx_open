#ifndef _eve_parse_servers_
#define _eve_parse_servers_

#include "api/Types.h"
#include "core/resource/resource.h"

void parseServers(QList<QnResourcePtr>& servers, const QnApiServers& xsdServers);

#endif // _eve_parse_servers_
