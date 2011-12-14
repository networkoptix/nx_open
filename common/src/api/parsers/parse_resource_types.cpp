#include <QStringList>

#include "api/parsers/parse_resource_types.h"

void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const QnApiResourceTypes& xsdResourceTypes)
{
    using xsd::api::resourceTypes::ResourceTypes;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (ResourceTypes::resourceType_const_iterator i (xsdResourceTypes.begin ());
             i != xsdResourceTypes.end ();
             ++i)
    {
        QnResourceTypePtr resourceType(new QnResourceType());

        resourceType->setId(i->id().c_str());
        resourceType->setName(i->name().c_str());
        if (i->manufacture().present())
            resourceType->setManufacture(i->manufacture()->c_str());

        if (i->parentIDs().present())
        {
            bool firstParent = true;
            for (ParentIDs::parentID_const_iterator parentsIter (i->parentIDs ()->parentID().begin ());
                     parentsIter != i->parentIDs ()->parentID().end ();
                     ++parentsIter)
            {
                if (firstParent)
                {
                    resourceType->setParentId(parentsIter->c_str());
                    firstParent = false;
                }
                else
                {
                    resourceType->addAdditionalParent(parentsIter->c_str());
                }
            }
        }

        if (i->propertyTypes().present())
        {
            for (PropertyTypes::propertyType_const_iterator propertyTypesIter (i->propertyTypes()->propertyType().begin ());
                     propertyTypesIter != i->propertyTypes()->propertyType().end ();
                     ++propertyTypesIter)
            {
                QnParamTypePtr param(new QnParamType());
                param->name = propertyTypesIter->name().c_str();
                param->type = (QnParamType::DataType)(ParamType::value)propertyTypesIter->type();

                if (propertyTypesIter->min().present())
                    param->min_val = *propertyTypesIter->min();

                if (propertyTypesIter->max().present())
                    param->max_val = *propertyTypesIter->max();

                if (propertyTypesIter->step().present())
                    param->step = *propertyTypesIter->step();

                if (propertyTypesIter->values().present())
                {
                    QString values = (*propertyTypesIter->values()).c_str();
                    foreach(QString val, values.split(QLatin1Char(',')))
                        param->possible_values.push_back(val.trimmed());
                }

                if (propertyTypesIter->ui_values().present())
                {
                    QString ui_values = (*propertyTypesIter->ui_values()).c_str();
                    foreach(QString val, ui_values.split(QLatin1Char(',')))
                        param->ui_possible_values.push_back(val.trimmed());
                }

                if (propertyTypesIter->default_value().present())
                    param->default_value = (*propertyTypesIter->default_value()).c_str();

                if (propertyTypesIter->netHelper().present())
                    param->paramNetHelper = (*propertyTypesIter->netHelper()).c_str();

                if (propertyTypesIter->group().present())
                    param->group = (*propertyTypesIter->group()).c_str();

                if (propertyTypesIter->sub_group().present())
                    param->subgroup = (*propertyTypesIter->sub_group()).c_str();

                if (propertyTypesIter->description().present())
                    param->description = (*propertyTypesIter->description()).c_str();

                param->ui = propertyTypesIter->ui();
                param->readonly = propertyTypesIter->readonly();

                resourceType->addParamType(param);
            }
        }

        resourceTypes.append(resourceType);
    }
}
