#ifndef QN_USER_DATA_I_H
#define QN_USER_DATA_I_H

#include "ec2_resource_data_i.h"

namespace ec2 {

    struct ApiUserData : ApiResourceData {
        ApiUserData(): isAdmin(false), rights(0) {}
    
        //QString password;
        bool isAdmin;
        qint64 rights;
        QString email;
        QByteArray digest;
        QByteArray hash; 

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiUserData_Fields (isAdmin)(rights)(email)(digest)(hash)

} // namespace ec2

#endif // QN_USER_DATA_I_H
