#ifndef __EC2_USER_DATA_H_
#define __EC2_USER_DATA_H_

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiUserData : ApiResourceData {
        ApiUserData(): isAdmin(false), permissions(0) {}

        //QString password;
        bool isAdmin;
        qint64 permissions;
        QString email;
        QnLatin1Array digest;
        QnLatin1Array hash; 
    };
#define ApiUserData_Fields ApiResourceData_Fields (isAdmin)(permissions)(email)(digest)(hash)

}

#endif // __EC2_USER_DATA_H_
