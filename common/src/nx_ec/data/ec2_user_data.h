#ifndef __EC2_USER_DATA_H_
#define __EC2_USER_DATA_H_

#include "ec2_resource_data.h"
#include "core/resource/resource_fwd.h"

namespace ec2
{

struct ApiUserData: public ApiResourceData
{
    QString password;
    bool isAdmin;
    qint64 rights;
    QString email;
    QString digest;
    QString hash; 

    void toResource(QnUserResourcePtr resource) const;
    QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS();
};

struct ApiUserDataList: public ApiData
{
    std::vector<ApiUserData> data;

    QN_DECLARE_STRUCT_SERIALIZATORS();

    void loadFromQuery(QSqlQuery& query);
    void toResourceList(QnUserResourceList& outData) const;
};

}

#define ApiUserDataFields (password) (isAdmin) (rights) (email) (digest) (hash)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiUserData, ApiUserDataFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiUserDataList, (data) )

#endif // __EC2_USER_DATA_H_
