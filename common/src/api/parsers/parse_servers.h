#ifndef _eve_parse_servers_
#define _eve_parse_servers_

#include "api/Types.h"
#include "core/resource/resource.h"
#include "core/resource/video_server.h"

typedef QSharedPointer<xsd::api::servers::Server> QnApiServerPtr;

QnApiServerPtr unparseServer(const QnVideoServer& serverIn);

template <class ResourcePtr>
void parseServers(QList<ResourcePtr> &servers, const QnApiServers &xsdServers, QnResourceFactory &/*resourceFactory*/)
{
    using xsd::api::servers::Servers;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Servers::server_const_iterator i (xsdServers.begin()); i != xsdServers.end(); ++i)
    {
        QnVideoServerPtr server(new QnVideoServer());
        server->setName(i->name().c_str());
        server->setId(i->id().c_str());
        server->setUrl(i->url().c_str());
        server->setGuid(i->guid().c_str());
        server->setApiUrl(i->apiUrl().c_str());

        servers.append(server);
    }
}

#endif // _eve_parse_servers_
