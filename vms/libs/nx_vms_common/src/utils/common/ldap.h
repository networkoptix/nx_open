// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/ldap_fwd.h>

const std::chrono::milliseconds kDefaultLdapPasswordExpirationPeriodMs = std::chrono::minutes(5);

struct NX_VMS_COMMON_API QnLdapSettings
{
    QUrl uri;
    QString adminDn;
    QString adminPassword;
    QString searchBase;
    QString searchFilter;
    std::chrono::milliseconds passwordExpirationPeriodMs = kDefaultLdapPasswordExpirationPeriodMs;
    int searchTimeoutS = 0;

    QString toString() const;
    bool isValid() const;

    static int defaultPort(bool ssl = false);
};

/**
* Used only to hide passwords from logs.
*/
NX_VMS_COMMON_API QString toString(const QnLdapSettings& settings);

#define QnLdapSettings_Fields \
    (uri)(adminDn)(adminPassword)(searchBase)(searchFilter)(passwordExpirationPeriodMs) \
    (searchTimeoutS)

struct NX_VMS_COMMON_API QnLdapUser
{
    QString dn;
    QString login;
    QString fullName;
    QString email;

    QString toString() const;
};

#define QnLdapUser_Fields (dn)(login)(fullName)(email)

QN_FUSION_DECLARE_FUNCTIONS(QnLdapSettings, (json)(metatype), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnLdapUser, (json)(metatype), NX_VMS_COMMON_API)

Q_DECLARE_METATYPE(QnLdapUsers)
