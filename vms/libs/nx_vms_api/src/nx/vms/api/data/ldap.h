// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/utils/void.h>

#include "data_macros.h"

namespace nx::vms::api {

using namespace std::chrono_literals;

const std::string kLdapScheme{"ldap"};
constexpr const int kLdapDefaultPort{389};

const std::string kLdapsScheme{"ldaps"};
constexpr const int kLdapsDefaultPort{636};

const int kLdapDefaultSearchPageSize{1000};

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

NX_REFLECTION_ENUM_CLASS(LdapSyncMode,
    /**%apidoc Nothing is synchronized. */
    disabled,

    /**%apidoc Only Groups are synchronized. */
    groupsOnly,

    /**%apidoc Users and Groups are synchronized. */
    usersAndGroups);

struct NX_VMS_API LdapSettings
{
    /**%apidoc:string
     * %example ldap://organization-server-address.com
     */
    nx::Url uri;

    /**%apidoc
     * Empty adminDn and empty adminPassword means using anonymous LDAP server bind for all
     * interactions with it.
     * %example cn=admin,dc=la
     */
    QString adminDn;

    /**%apidoc
     * %example password123
     */
    std::optional<QString> adminPassword;

    /**%apidoc[opt]
     * LDAP attribute for name field in VMS DB. Autodetected by VMS if not specified
     *     (Active Directory: sAMAccountName, Open LDAP: uid).
     * %example uid
     */
    QString loginAttribute;

    /**%apidoc[opt]
     * LDAP objectClass attribute to detect Groups. Autodetected by VMS if not specified
     *     (Active Directory: group, Open LDAP: groupOfNames).
     * %example groupOfNames
     */
    QString groupObjectClass;

    /**%apidoc[opt]
     * LDAP Group attribute to detect it's members. Autodetected by VMS if not specified
     *     (Active Directory: member, Open LDAP: member).
     * %example member
     */
    QString memberAttribute;

    /**%apidoc[opt] */
    std::chrono::milliseconds passwordExpirationPeriodMs = 5min;

    /**%apidoc[opt] Timeout for LDAP syncs. */
    std::chrono::seconds searchTimeoutS = 1min;

    /**%apidoc[opt] Timeout for LDAP search responses. */
    std::chrono::seconds responseTimeoutS = 30s;

    /**%apidoc[opt] */
    int searchPageSize = kLdapDefaultSearchPageSize;

    /**%apidoc[opt] LDAP users and groups are collected using all these filters. */
    std::vector<LdapSettingSearchFilter> filters;

    /**%apidoc[opt] Automatic synchronization policy. */
    LdapSyncMode continuousSync = LdapSyncMode::groupsOnly;

    /**%apidoc[opt] Delay between automatic synchronization cycles.
     * %example 0
     */
    std::chrono::seconds continuousSyncIntervalS = 1h;

    /**%apidoc[opt]
     * Preferred Server in the Site that will be used to execute synchronization with the LDAP
     * Server if it is online and has connection with the LDAP Server.
     */
    nx::Uuid preferredMasterSyncServer;

    /**%apidoc[opt]
     * %example 10
     */
    std::chrono::seconds masterSyncServerCheckIntervalS = 10s;

    /**%apidoc[opt]
     * If `true` new LDAP users will be imported with enabled HTTP Digest.
     */
    bool isHttpDigestEnabledOnImport = false;

    /**%apidoc[opt]
     * If `true` use TLS protocol to communicate with LDAP server.
     */
    bool startTls = false;

    /**%apidoc[opt]
     * If `true` ignore SSL/TLS certificate errors when connecting to LDAP server.
     */
    bool ignoreCertificateErrors = false;

    /**%apidoc[opt] Remove all existing LDAP users and groups from VMS. */
    bool removeRecords = false;

    /**%apidoc[opt]
     * Locale for all newly imported users. Empty value will be treated as a default Site locale.
     * %example en_US
     */
    QString defaultUserLocale;

    bool operator==(const LdapSettings&) const = default;
    nx::utils::Void getId() const { return {}; }

    bool isValid(bool checkPassword = true) const;
    bool isEmpty() const;
    QString syncId() const;

    static int defaultPort(bool useSsl = false);
};
#define LdapSettings_Fields \
    (uri) \
    (adminDn) \
    (adminPassword) \
    (loginAttribute) \
    (groupObjectClass) \
    (memberAttribute) \
    (passwordExpirationPeriodMs) \
    (searchTimeoutS) \
    (responseTimeoutS) \
    (searchPageSize) \
    (filters) \
    (continuousSync) \
    (continuousSyncIntervalS) \
    (preferredMasterSyncServer) \
    (masterSyncServerCheckIntervalS) \
    (isHttpDigestEnabledOnImport) \
    (removeRecords) \
    (startTls) \
    (ignoreCertificateErrors) \
    (defaultUserLocale)

QN_FUSION_DECLARE_FUNCTIONS(LdapSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LdapSettings, LdapSettings_Fields)

struct NX_VMS_API LdapStatus
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(State,
        online,
        unstable,
        offline);

    /**%apidoc Server connection state. */
    State state = State::offline;

    bool isRunning = false;

    /**%apidoc What's being synced right now. */
    LdapSyncMode mode = LdapSyncMode::disabled;
    QString message;
    std::optional<std::chrono::seconds> timeSinceSyncS;

    bool operator==(const LdapStatus&) const = default;
};
#define LdapStatus_Fields (state)(isRunning)(mode)(message)(timeSinceSyncS)
NX_VMS_API_DECLARE_STRUCT_EX(LdapStatus, (json))
NX_REFLECTION_INSTRUMENT(LdapStatus, LdapStatus_Fields)

} // namespace nx::vms::api
