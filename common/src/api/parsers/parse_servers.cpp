#include "api/parsers/parse_servers.h"

#include "core/resourcemanagment/asynch_seacher.h"

void parseServers(QList<QnVideoServerPtr>& servers, const QnApiServers& xsdServers, QnResourceFactory& resourceFactory)
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
        server->setApiUrl(i->apiUrl().c_str());

        servers.append(server);
    }
}
