#include "api/parsers/parse_layouts.h"

void parseLayouts(QList<QnResourcePtr>& layouts, const QnApiLayouts& xsdLayouts)
{
    using xsd::api::layouts::Layouts;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Layouts::layout_const_iterator i (xsdLayouts.begin()); i != xsdLayouts.end(); ++i)
    {
        QnResourceParameters parameters;
        parameters["id"] = i->id().c_str();
        parameters["name"] = i->name().c_str();

        QnResourcePtr layout = QnResourceFactoryPool::createResource(i->typeId().c_str(), parameters);

        layouts.append(layout);
    }
}

