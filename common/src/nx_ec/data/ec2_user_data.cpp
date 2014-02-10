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
    
    template <class T>
    void ApiUserDataList::toResourceList(QList<T>& outData) const
    {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnUserResourcePtr user(new QnUserResource());
            data[i].toResource(user);
            outData << user;
        }
    }
    template void ApiUserDataList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiUserDataList::toResourceList<QnUserResourcePtr>(QList<QnUserResourcePtr>& outData) const;

    void ApiUserDataList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiUserData, data, ApiUserDataFields ApiResourceDataFields)
    }

}
