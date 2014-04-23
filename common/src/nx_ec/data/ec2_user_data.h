#ifndef __EC2_USER_DATA_H_
#define __EC2_USER_DATA_H_

#include "ec2_resource_data.h"
#include "core/resource/resource_fwd.h"
#include "ec2_user_data_i.h"

namespace ec2
{
    void fromApiToResource(const ApiUserData& data, QnUserResourcePtr resource);
    void fromResourceToApi(const QnUserResourcePtr resource, ApiUserData& data);

    QN_DEFINE_STRUCT_SQL_BINDER(ApiUserData, ApiUserFields);

    struct ApiUserList: public ApiUserDataListData
    {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };
}

#endif // __EC2_USER_DATA_H_
