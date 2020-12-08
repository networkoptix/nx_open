#pragma once

#include <QtCore/QUrl>

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/ldap_fwd.h>

const std::chrono::milliseconds kDefaultLdapPasswordExperationPeriod = std::chrono::minutes(5);

struct QnLdapSettings
{
    QUrl uri;
    QString adminDn;
    QString adminPassword;
    QString searchBase;
    QString searchFilter;
    std::chrono::milliseconds passwordExperationPeriodMs = kDefaultLdapPasswordExperationPeriod;
    int searchTimeoutS = 0;

    QString toString() const;
    bool isValid() const;

    static int defaultPort(bool ssl = false);
};

/**
* Used only to hide passwords from logs.
*/
QString toString(const QnLdapSettings& settings);

#define QnLdapSettings_Fields (uri)(adminDn)(adminPassword)(searchBase)(searchFilter)(searchTimeoutS)

struct QnLdapUser
{
    QString dn;
    QString login;
    QString fullName;
    QString email;

    QString toString() const;
};

#define QnLdapUser_Fields (dn)(login)(fullName)(email)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnLdapSettings)(QnLdapUser), (json)(eq)(metatype))

Q_DECLARE_METATYPE(QnLdapUsers)
