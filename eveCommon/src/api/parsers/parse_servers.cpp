#include "api/parsers/parse_servers.h"

void parseServers(QList<QnResourcePtr>& servers, const QnApiServers& xsdServers)
{
    using xsd::api::servers::Servers;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Servers::server_const_iterator i (xsdServers.begin()); i != xsdServers.end(); ++i)
    {
        QnResourceParameters parameters;
        parameters["id"] = i->id().c_str();
        parameters["name"] = i->name().c_str();

        QnResourcePtr server = QnResourceFactoryPool::createResource(i->typeId().c_str(), parameters);

        servers.append(server);
    }
}
