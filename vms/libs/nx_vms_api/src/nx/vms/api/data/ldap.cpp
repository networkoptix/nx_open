// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

bool LdapSettings::isValid(bool checkPassword) const
{
    return !uri.isEmpty()
        && (!checkPassword || !(adminDn.isEmpty() ^ adminPassword.value_or(QString()).isEmpty()));
}

int LdapSettings::defaultPort(bool useSsl)
{
    constexpr int kDefaultLdapPort = 389;
    constexpr int kDefaultLdapSslPort = 636;
    return useSsl ? kDefaultLdapSslPort : kDefaultLdapPort;
}

QString LdapSettings::syncId() const
{
    if (!isValid(/*checkPassword*/ false))
        return QString();

    QString data = uri.toString()
        + adminDn
        + loginAttribute.value_or(QString())
        + groupObjectClass.value_or(QString())
        + memberAttribute.value_or(QString());
    std::set<QString> uniqueFilters;
    for (auto filter: filters)
        uniqueFilters.insert(filter.base + filter.filter);
    for (auto filter: uniqueFilters)
        data += filter;
    return QnUuid::fromArbitraryData(data.toUtf8()).toSimpleString();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettingSearchFilter, (json), LdapSettingSearchFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettings, (json), LdapSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettingsChange, (json), LdapSettingsChange_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapStatus, (json), LdapStatus_Fields)

} // namespace nx::vms::api
