#ifndef __EC2_USER_DATA_H_
#define __EC2_USER_DATA_H_

#include "api_globals.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiUserData : ApiResourceData {
        ApiUserData():
            isAdmin(false),
            permissions(Qn::NoGlobalPermissions),
            isLdap(false),
            isEnabled(true),
            isCloud(false)
        {}

        /** Really this flag must be named isOwner, but we must keep it to maintain mobile client compatibility. */
        bool isAdmin;

        /** Global user permissions. */
        Qn::GlobalPermissions permissions;

        /** Id of the access rights group. */
        QnUuid groupId;

        QString email;
        QnLatin1Array digest;
        QnLatin1Array hash;
        //!Hash suitable to be used in /etc/shadow file
        QnLatin1Array cryptSha512Hash;
        QString realm;
		bool isLdap;
		bool isEnabled;

        /** Flag if user is created from the Cloud. */
        bool isCloud;
    };
#define ApiUserData_Fields ApiResourceData_Fields (isAdmin)(permissions)(email)(digest)(hash)(cryptSha512Hash)(realm)(isLdap)(isEnabled)(groupId)(isCloud)

}

#endif // __EC2_USER_DATA_H_
