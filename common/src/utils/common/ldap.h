#ifndef QN_LDAP_H
#define QN_LDAP_H

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/ldap_fwd.h>

struct QnLdapSettings {
    QUrl uri;
    QString adminDn;
    QString adminPassword;
    QString searchBase;
    QString searchFilter;

    bool isValid() const;

    static int defaultPort(bool ssl = false);
};

#define QnLdapSettings_Fields (uri)(adminDn)(adminPassword)(searchBase)(searchFilter)

struct QnLdapUser {
    QString dn;
    QString login;
    QString fullName;
    QString email;
};

#define QnLdapUser_Fields (dn)(login)(fullName)(email)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnLdapSettings)(QnLdapUser), (json)(eq)(metatype))

Q_DECLARE_METATYPE(QnLdapUsers)

#endif // QN_LDAP_H