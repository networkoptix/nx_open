#ifndef __EC2_USER_DATA_H_
#define __EC2_USER_DATA_H_

#include "ec2_resource_data.h"
#include "core/resource/resource_fwd.h"

namespace ec2
{

struct ApiUserData: public ApiResourceData
{
    ApiUserData(): isAdmin(false), rights(0) {}
    
    //QString password;
    bool isAdmin;
    qint64 rights;
    QString email;
    QByteArray digest;
    QByteArray hash; 

    void toResource(QnUserResourcePtr resource) const;
    void fromResource(QnUserResourcePtr resource);
    QN_DECLARE_STRUCT_SQL_BINDER();
};

struct ApiUserDataList: public ApiData
{
    std::vector<ApiUserData> data;

    void loadFromQuery(QSqlQuery& query);
    template <class T> void toResourceList(QList<T>& outData) const;
};

}

#define ApiUserDataFields (isAdmin) (rights) (email) (digest) (hash)
QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS(ec2::ApiUserData, ec2::ApiResourceData, ApiUserDataFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiUserDataList, (data) )

#endif // __EC2_USER_DATA_H_
