#include "api/parsers/parse_users.h"

#include "core/resource/resource.h"

void parseUsers(QList<QnResourcePtr>& users, const QnApiUsers& xsdUsers, QnResourceFactory& resourceFactory)
{
    using xsd::api::users::Users;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Users::user_const_iterator i (xsdUsers.begin()); i != xsdUsers.end(); ++i)
    {
        QnResourceParameters parameters;
        parameters["id"] = i->id().c_str();
        parameters["name"] = i->name().c_str();

        QnResourcePtr user = resourceFactory.createResource(i->typeId().c_str(), parameters);

        users.append(user);
    }
}


