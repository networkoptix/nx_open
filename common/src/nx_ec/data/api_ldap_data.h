#ifndef QN_API_LDAP_DATA_H
#define QN_API_LDAP_DATA_H

#include "api_globals.h"
#include "api_data.h"

#include <utils/common/ldap_fwd.h>

namespace ec2
{
    struct ApiLdapSettingsData: ApiData
    {
        ApiLdapSettingsData() {}

        QString host;
        int port;
        QString adminDn;
        QString adminPassword;
        QString searchBase;
    };
#define ApiLdapSettingsData_Fields (host)(port)(adminDn)(adminPassword)(searchBase)
} // namespace ec2

#endif // QN_API_LDAP_DATA_H
