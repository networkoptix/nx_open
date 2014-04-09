#ifndef __EC2_USER_DATA_H_
#define __EC2_USER_DATA_H_

#include "ec2_resource_data.h"
#include "core/resource/resource_fwd.h"

namespace ec2
{
    struct ApiUser;
    #include "ec2_user_data_i.h"

    struct ApiUser: ApiUserData, ApiResource
    {
        void toResource(QnUserResourcePtr resource) const;
        void fromResource(QnUserResourcePtr resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };
    QN_DEFINE_STRUCT_SQL_BINDER(ApiUser, ApiUserFields);

    struct ApiUserList: public ApiUserListData
    {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };
}

#endif // __EC2_USER_DATA_H_
