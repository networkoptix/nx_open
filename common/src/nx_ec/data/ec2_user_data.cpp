#include "ec2_user_data.h"
#include "core/resource/user_resource.h"

namespace ec2
{

    void ApiUserData::toResource(QnUserResourcePtr resource) const
    {
        ApiResourceData::toResource(resource);
        resource->setPassword(password);
        resource->setAdmin(isAdmin);
        resource->setPermissions(rights);
        resource->setEmail(email);
        resource->setDigest(digest);
        resource->setHash(hash);
    }
    
    void ApiUserDataList::toResourceList(QnUserResourceList& outData) const
    {
        outData.reserve(data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnUserResourcePtr user(new QnUserResource());
            data[i].toResource(user);
            outData << user;
        }
    }

    void ApiUserDataList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(ApiUserData, data, ApiUserDataFields ApiResourceDataFields)
    }

}
