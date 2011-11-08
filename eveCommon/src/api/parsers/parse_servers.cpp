#include "api/parsers/parse_servers.h"

#include "core/resourcemanagment/asynch_seacher.h"

void parseServers(QList<QnServerPtr>& servers, const QnApiServers& xsdServers, QnResourceFactory& resourceFactory)
{
    using xsd::api::servers::Servers;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Servers::server_const_iterator i (xsdServers.begin()); i != xsdServers.end(); ++i)
    {
        QnServerPtr server(new QnServer(i->name().c_str()));
        server->setId(i->id().c_str());
        server->setUrl(i->id().c_str());
        server->setMAC(QString(i->mac().c_str()));

        servers.append(server);
    }
}
