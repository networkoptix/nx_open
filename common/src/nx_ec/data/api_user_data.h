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
        qint64 rights; // TODO: #API rename 'permissions' for consistency with other parts of the system.
        QString email;
        QByteArray digest;
        QByteArray hash; 
    };
#define ApiUserData_Fields ApiResourceData_Fields (isAdmin)(rights)(email)(digest)(hash)

}

#endif // __EC2_USER_DATA_H_
