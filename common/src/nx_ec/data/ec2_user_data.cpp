#include "ec2_user_data.h"
#include "core/resource/user_resource.h"

namespace ec2
{
    void fromApiToResource(const ApiUserData& data, QnUserResourcePtr resource)
    {
        fromApiToResource((const ApiResourceData &)data, resource);
        resource->setAdmin(data.isAdmin);
        resource->setEmail(data.email);
        resource->setHash(data.hash);

        resource->setPermissions(data.rights);
        resource->setDigest(data.digest);
    }
    
    void fromResourceToApi(const QnUserResourcePtr resource, ApiUserData& data)
    {
        fromResourceToApi(resource, (ApiResourceData &)data);
        QString password = resource->getPassword();
        
        if (!password.isEmpty()) {
            QByteArray salt = QByteArray::number(rand(), 16);
            QCryptographicHash md5(QCryptographicHash::Md5);
            md5.addData(salt);
            md5.addData(password.toUtf8());
            data.hash = "md5$";
            data.hash.append(salt);
            data.hash.append("$");
            data.hash.append(md5.result().toHex());
        
            md5.reset();
            md5.addData(QString(lit("%1:NetworkOptix:%2")).arg(resource->getName(), password).toLatin1());
            data.digest = md5.result().toHex();
        }

        data.isAdmin = resource->isAdmin();
        data.rights = resource->getPermissions();
        data.email = resource->getEmail();
    }

    template <class T>
    void ApiUserList::toResourceList(QList<T>& outData) const
    {
        outData.reserve(outData.size() + data.size());
        for(int i = 0; i < data.size(); ++i) 
        {
            QnUserResourcePtr user(new QnUserResource());
            fromApiToResource(data[i], user);
            outData << user;
        }
    }
    template void ApiUserList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData) const;
    template void ApiUserList::toResourceList<QnUserResourcePtr>(QList<QnUserResourcePtr>& outData) const;

    void ApiUserList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiUserData, data, ApiUserFields ApiResourceFields)
    }

}
