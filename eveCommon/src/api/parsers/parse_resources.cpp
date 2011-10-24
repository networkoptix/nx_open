#include "api/parsers/parse_resources.h"

void parseResources(QList<QnResourcePtr>& resources, const QnApiResources& xsdResources)
{
    using xsd::api::resources::Resources;

    for (Resources::resource_const_iterator i (xsdResources.begin()); i != xsdResources.end(); ++i)
    {
        QnResourceParameters parameters;
        parameters["id"] = i->id().c_str();
        parameters["name"] = i->name().c_str();

        QnResourcePtr resource = QnResourceFactoryPool::createResource(i->typeId().c_str(), parameters);

        resources.append(resource);
    }
}

