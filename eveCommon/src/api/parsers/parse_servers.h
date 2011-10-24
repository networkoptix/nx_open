#ifndef _eve_parse_servers_
#define _eve_parse_servers_

#include "device/qnresource.h"
#include "api/Types.h"

void parseServers(QList<QnResourcePtr>& servers, const QnApiServers& xsdServers);

#endif // _eve_parse_servers_
