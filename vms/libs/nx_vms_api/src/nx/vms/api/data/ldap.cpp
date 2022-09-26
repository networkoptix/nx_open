// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

const QString kDefaultLdapLoginAttribute = QStringLiteral("uid");
const QString kDefaultLdapGroupObjectClass = QStringLiteral("groupOfNames");
const QString kDefaultLdapMemberAttribute = QStringLiteral("member");

bool LdapSettingsBase::isValid(bool checkPassword) const
{
    return !uri.isEmpty() && !adminDn.isEmpty() && !(checkPassword && adminPassword.isEmpty());
}

int LdapSettingsBase::defaultPort(bool useSsl)
{
    constexpr int kDefaultLdapPort = 389;
    constexpr int kDefaultLdapSslPort = 636;
    return useSsl ? kDefaultLdapSslPort : kDefaultLdapPort;
}

LdapSettingsDeprecated::LdapSettingsDeprecated(LdapSettings settings)
{
    static_cast<LdapSettingsBase&>(*this) = static_cast<LdapSettingsBase&&>(settings);
    if (!settings.filters.empty())
    {
        searchBase = settings.filters[0].base;
        searchFilter = settings.filters[0].userFilter;
    }
}

LdapSettingsDeprecated::operator LdapSettings() &&
{
    LdapSettings r;
    static_cast<LdapSettingsBase&>(r) = static_cast<LdapSettingsBase&&>(*this);
    if (!searchBase.isEmpty() || !searchFilter.isEmpty())
        r.filters.push_back({.base = searchBase, .groupFilter = {}, .userFilter = searchFilter});
    return r;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettingsDeprecated, (json), LdapSettingsDeprecated_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettingSearchFilter, (json), LdapSettingSearchFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettings, (json), LdapSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapUser, (json), LdapUser_Fields)

} // namespace nx::vms::api
