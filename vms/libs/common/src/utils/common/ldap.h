#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/ldap_fwd.h>

struct QnLdapSettings
{
    QUrl uri;
    QString adminDn;
    QString adminPassword;
    QString searchBase;
    QString searchFilter;
    int searchTimeoutS = 0;

    QString toString(bool hidePassword = false) const;
    bool isValid() const;

    static int defaultPort(bool ssl = false);
};

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
