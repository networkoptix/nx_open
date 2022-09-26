// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include "data_macros.h"
#include "void.h"

namespace nx::vms::api {

using namespace std::chrono_literals;

NX_VMS_API extern const QString kDefaultLdapLoginAttribute;
NX_VMS_API extern const QString kDefaultLdapGroupObjectClass;
NX_VMS_API extern const QString kDefaultLdapMemberAttribute;
constexpr std::chrono::minutes kDefaultLdapPasswordExpirationPeriod = 5min;
constexpr std::chrono::seconds kDefaultLdapSearchTimeout = 30s;
constexpr int kDefaultLdapSearchPageSize = 100;

struct NX_VMS_API LdapSettingsBase
{
    /**%apidoc:string */
    QUrl uri;

    QString adminDn;
    QString adminPassword;

    /**%apidoc[opt]
     * LDAP User login attribute.
     * %value "uid"
     */
    QString loginAttribute = kDefaultLdapLoginAttribute;

    /**%apidoc[opt]
     * LDAP Group objectClass.
     * %value "groupOfNames"
     * %value "groupOfUniqueNames"
     */
    QString groupObjectClass = kDefaultLdapGroupObjectClass;

    /**%apidoc[opt]
     * LDAP Group member attribute.
     * %value "member"
     * %value "uniqueMember"
     */
    QString memberAttribute = kDefaultLdapMemberAttribute;

    /**%apidoc[opt] */
    std::chrono::milliseconds passwordExpirationPeriodMs = kDefaultLdapPasswordExpirationPeriod;

    /**%apidoc[opt] */
    std::chrono::seconds searchTimeoutS = kDefaultLdapSearchTimeout;

    /**%apidoc[opt] */
    int searchPageSize = kDefaultLdapSearchPageSize;

    bool operator==(const LdapSettingsBase&) const = default;
    bool isValid(bool checkPassword = true) const;

    static int defaultPort(bool useSsl = false);
};
#define LdapSettingsBase_Fields \
    (uri) \
    (adminDn) \
    (adminPassword) \
    (loginAttribute) \
    (groupObjectClass) \
    (memberAttribute) \
    (passwordExpirationPeriodMs) \
    (searchTimeoutS) \
    (searchPageSize)

struct NX_VMS_API LdapSettingSearchFilter
{
    QString base;
    QString groupFilter;
    QString userFilter;

    bool operator==(const LdapSettingSearchFilter&) const = default;
};
#define LdapSettingSearchFilter_Fields (base)(groupFilter)(userFilter)
QN_FUSION_DECLARE_FUNCTIONS(LdapSettingSearchFilter, (json), NX_VMS_API)

struct NX_VMS_API LdapSettings: LdapSettingsBase
{
    /**%apidoc[opt] LDAP users and groups are collected using all these filters. */
    std::vector<LdapSettingSearchFilter> filters;

    bool operator==(const LdapSettings&) const = default;
    Void getId() const { return Void(); }
};
#define LdapSettings_Fields LdapSettingsBase_Fields(filters)
QN_FUSION_DECLARE_FUNCTIONS(LdapSettings, (json), NX_VMS_API)

// TODO: Remove this struct after `/api/testLdapSettings` support is dropped.
struct NX_VMS_API LdapSettingsDeprecated: LdapSettingsBase
{
    /**%apidoc[opt] */
    QString searchBase;

    /**%apidoc[opt] LDAP User search filter. */
    QString searchFilter;

    LdapSettingsDeprecated() = default;
    LdapSettingsDeprecated(LdapSettings settings);
    operator LdapSettings() &&;
    bool operator==(const LdapSettingsDeprecated&) const = default;
};
#define LdapSettingsDeprecated_Fields LdapSettingsBase_Fields(searchBase)(searchFilter)
QN_FUSION_DECLARE_FUNCTIONS(LdapSettingsDeprecated, (json), NX_VMS_API)

// TODO: Remove this struct after `/api/testLdapSettings` support is dropped.
struct NX_VMS_API LdapUser
{
    QString dn;
    QString login;
    QString fullName;
    QString email;
};
#define LdapUser_Fields (dn)(login)(fullName)(email)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(LdapUser, (json))

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::LdapSettings)
Q_DECLARE_METATYPE(nx::vms::api::LdapUser)
Q_DECLARE_METATYPE(nx::vms::api::LdapUserList)
