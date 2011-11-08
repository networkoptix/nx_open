#ifndef _eve_parse_servers_
#define _eve_parse_servers_

#include "api/Types.h"
#include "core/resource/resource.h"
#include "core/resource/server.h"

void parseServers(QList<QnServerPtr>& servers, const QnApiServers& xsdServers, QnResourceFactory& resourceFactory);

#endif // _eve_parse_servers_
