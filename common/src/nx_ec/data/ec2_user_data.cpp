#include "ec2_user_data.h"
#include "core/resource/user_resource.h"

namespace ec2
{

    void ApiUser::toResource(QnUserResourcePtr resource) const
    {
        ApiResource::toResource(resource);
        resource->setAdmin(isAdmin);
        resource->setEmail(email);
        resource->setHash(hash);

        resource->setPermissions(rights);
        resource->setDigest(digest);
    }
    
    void ApiUser::fromResource(QnUserResourcePtr resource)
    {
        ApiResource::fromResource(resource);
        QString password = resource->getPassword();
        
        if (!password.isEmpty()) {
            QByteArray salt = QByteArray::number(rand(), 16);
            QCryptographicHash md5(QCryptographicHash::Md5);
            md5.addData(salt);
            md5.addData(password.toUtf8());
            hash = "md5$";
            hash.append(salt);
            hash.append("$");
            hash.append(md5.result().toHex());
        
            md5.reset();
            md5.addData(QString(lit("%1:NetworkOptix:%2")).arg(resource->getName(), password).toLatin1());
            digest = md5.result().toHex();
        }

        isAdmin = resource->isAdmin();
        rights = resource->getPermissions();
        email = resource->getEmail();
    }

    template <class T>
    void ApiUserList::toResourceList(QList<T>& outData) const
    {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnUserResourcePtr user(new QnUserResource());
            data[i].toResource(user);
            outData << user;
        }
    }
    template void ApiUserList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiUserList::toResourceList<QnUserResourcePtr>(QList<QnUserResourcePtr>& outData) const;

    void ApiUserList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiUser, data, ApiUserFields ApiResourceFields)
    }

}
