// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

const QString kDefaultLdapLoginAttribute = QStringLiteral("uid");
const QString kDefaultLdapGroupObjectClass = QStringLiteral("groupOfNames");
const QString kDefaultLdapMemberAttribute = QStringLiteral("member");

bool LdapSettings::isValid(bool checkPassword) const
{
    return !uri.isEmpty() && !adminDn.isEmpty() && !(checkPassword && adminPassword.isEmpty());
}

int LdapSettings::defaultPort(bool useSsl)
{
    constexpr int kDefaultLdapPort = 389;
    constexpr int kDefaultLdapSslPort = 636;
    return useSsl ? kDefaultLdapSslPort : kDefaultLdapPort;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettings, (json), LdapSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapUser, (json), LdapUser_Fields)

} // namespace nx::vms::api
