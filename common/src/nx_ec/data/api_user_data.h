#ifndef __EC2_USER_DATA_H_
#define __EC2_USER_DATA_H_

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiUserData : ApiResourceData {
        ApiUserData(): isAdmin(false), permissions(0) {}

        bool isAdmin;
        qint64 permissions;
        QString email;
        QnLatin1Array digest;
        QnLatin1Array hash;
        //!Hash suitable to be used in /etc/shadow file
        QnLatin1Array cryptSha512Hash;
		bool isLdap;
		bool isEnabled;
    };
#define ApiUserData_Fields ApiResourceData_Fields (isAdmin)(permissions)(email)(digest)(hash)(cryptSha512Hash)(isLdap)(isEnabled)

}

#endif // __EC2_USER_DATA_H_
