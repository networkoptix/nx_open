#ifndef __EC2_USER_DATA_H_
#define __EC2_USER_DATA_H_

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiUserData : ApiResourceData {
        ApiUserData(): isAdmin(false), rights(0) {}

        //QString password;
        bool isAdmin;
        qint64 rights;
        QString email;
        QByteArray digest;
        QByteArray hash; 

        //QN_DECLARE_STRUCT_SQL_BINDER();
    };
#define ApiUserData_Fields (isAdmin)(rights)(email)(digest)(hash)

    /*void fromApiToResource(const ApiUserData& data, QnUserResourcePtr resource);
    void fromResourceToApi(const QnUserResourcePtr resource, ApiUserData& data);*/

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiUserData, ApiUserFields);

    /*struct ApiUserList: public std::vector<ApiUserData>
    {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };*/
}

#endif // __EC2_USER_DATA_H_
