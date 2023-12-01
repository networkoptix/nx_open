// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

bool LdapSettings::isValid(bool checkPassword) const
{
    return uri.isValid()
        && (!checkPassword || !(adminDn.isEmpty() ^ adminPassword.value_or(QString()).isEmpty()));
}

bool LdapSettings::isEmpty() const
{
    return *this == LdapSettings{.removeRecords = removeRecords};
}

int LdapSettings::defaultPort(bool useSsl)
{
    constexpr int kDefaultLdapPort = 389;
    constexpr int kDefaultLdapSslPort = 636;
    return useSsl ? kDefaultLdapSslPort : kDefaultLdapPort;
}

QString LdapSettings::syncId() const
{
    const auto cmpFilters =
        [](const api::LdapSettingSearchFilter& lhs, const api::LdapSettingSearchFilter& rhs)
        {
            return std::tie(lhs.base, lhs.filter) < std::tie(rhs.base, rhs.filter);
        };

    if (!isValid(/*checkPassword*/ false))
        return {};

    QString data = uri.toString() + adminDn + loginAttribute + groupObjectClass + memberAttribute;
    for (const auto& filter: nx::utils::unique_sorted(filters, cmpFilters))
        data += filter.base + filter.filter;
    return QnUuid::fromArbitraryData(data.toUtf8()).toSimpleString();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettingSearchFilter, (json), LdapSettingSearchFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapSettings, (json), LdapSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LdapStatus, (json), LdapStatus_Fields)

} // namespace nx::vms::api
