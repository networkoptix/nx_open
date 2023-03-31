// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"
#include "void.h"

namespace nx::vms::api {

using namespace std::chrono_literals;

NX_VMS_API extern const QString kDefaultLdapLoginAttribute;
NX_VMS_API extern const QString kDefaultLdapGroupObjectClass;
NX_VMS_API extern const QString kDefaultLdapMemberAttribute;
constexpr std::chrono::minutes kDefaultLdapPasswordExpirationPeriod = 5min;
constexpr std::chrono::seconds kDefaultLdapSearchTimeout = 30s;
constexpr int kDefaultLdapSearchPageSize = 1000;

struct NX_VMS_API LdapSettingsBase
{
    /**%apidoc:string
     * %example ldap://organization-server-address.com
     */
    nx::utils::Url uri;

    /**%apidoc
     * %example cn=admin,dc=la
     */
    QString adminDn;

    /**%apidoc
     * %example password123
     */
    std::optional<QString> adminPassword;

    /**%apidoc[opt]
     * LDAP User login attribute.
     * %value "uid"
     * %example uid
     */
    QString loginAttribute = kDefaultLdapLoginAttribute;

    /**%apidoc[opt]
     * LDAP Group objectClass.
     * %value "groupOfNames"
     * %value "groupOfUniqueNames"
     * %example groupOfNames
     */
    QString groupObjectClass = kDefaultLdapGroupObjectClass;

    /**%apidoc[opt]
     * LDAP Group member attribute.
     * %value "member"
     * %value "uniqueMember"
     * %example member
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
    /**%apidoc[opt]
     * %example Users
     */
    QString name;

    /**%apidoc
     * %example ou=users,dc=la
     */
    QString base;

    /**%apidoc[opt] */
    QString filter;

    bool operator==(const LdapSettingSearchFilter&) const = default;
};
#define LdapSettingSearchFilter_Fields (name)(base)(filter)
QN_FUSION_DECLARE_FUNCTIONS(LdapSettingSearchFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LdapSettingSearchFilter, LdapSettingSearchFilter_Fields)

struct NX_VMS_API LdapSettings: LdapSettingsBase
{
    /**%apidoc[opt] LDAP users and groups are collected using all these filters. */
    std::vector<LdapSettingSearchFilter> filters;

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Sync,
        disabled,
        groupsOnly,
        usersAndGroups);

    /**%apidoc[opt] */
    Sync continuousSync = Sync::disabled;

    /**%apidoc[opt]
     * %example 0
     */
    std::chrono::seconds continuousSyncIntervalS = 0s;

    /**%apidoc[opt]
     * Preferred Server in the System that will be used to execute synchronization with the LDAP
     * Server if it is online and has connection with the LDAP Server.
     */
    QnUuid preferredMasterSyncServer;

    /**%apidoc[opt]
     * %example 10
     */
    std::chrono::seconds masterSyncServerCheckIntervalS = 10s;

    bool operator==(const LdapSettings&) const = default;
    Void getId() const { return Void(); }
};
#define LdapSettings_Fields LdapSettingsBase_Fields \
    (filters) \
    (continuousSync) \
    (continuousSyncIntervalS) \
    (preferredMasterSyncServer) \
    (masterSyncServerCheckIntervalS)
QN_FUSION_DECLARE_FUNCTIONS(LdapSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LdapSettings, LdapSettings_Fields)

struct NX_VMS_API LdapSettingsChange: LdapSettings
{
    /**%apidoc[opt] Remove all existing LDAP users and groups from VMS. */
    bool removeRecords = false;
};
#define LdapSettingsChange_Fields LdapSettings_Fields (removeRecords)
QN_FUSION_DECLARE_FUNCTIONS(LdapSettingsChange, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LdapSettingsChange, LdapSettingsChange_Fields)

// TODO: Remove this struct after `/api/testLdapSettings` support is dropped.
struct NX_VMS_API LdapSettingsDeprecated: LdapSettingsBase
{
    /**%apidoc[opt] */
    QString searchBase;

    /**%apidoc[opt] LDAP User search filter. */
    QString searchFilter;

    LdapSettingsDeprecated() = default;
    LdapSettingsDeprecated(LdapSettings settings);
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

struct NX_VMS_API LdapStatus
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(State,
        online,
        unstable,
        offline);

    /**%apidoc Server connection state. */
    State state = State::offline;

    bool isRunning = false;
    QString message;
    std::optional<std::chrono::seconds> timeSinceSyncS;
};
#define LdapStatus_Fields (state)(isRunning)(message)(timeSinceSyncS)
NX_VMS_API_DECLARE_STRUCT_EX(LdapStatus, (json))
NX_REFLECTION_INSTRUMENT(LdapStatus, LdapStatus_Fields)

} // namespace nx::vms::api

