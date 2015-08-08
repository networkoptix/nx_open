#ifndef __API_MERGE_LDAP_USERS_DATA_H_
#define __API_MERGE_LDAP_USERS_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiMergeLdapUsersData : ApiData
    {
        ApiMergeLdapUsersData() {}

        int dummy;
    };
#define ApiMergeLdapUsersData_Fields (dummy)
}

#endif // __API_MERGE_LDAP_USERS_DATA_H_


