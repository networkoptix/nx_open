#ifndef LDAP_MANAGER_H_
#define LDAP_MANAGER_H_

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>

#include <utils/common/singleton.h>
#include <utils/common/ldap.h>
#include <utils/thread/mutex.h>
#include "common/common_globals.h"

class QnLdapManagerPrivate;

namespace Qn {
    enum LdapResult {
        Ldap_NoError = 0,
        Ldap_SizeLimit,
        Ldap_InvalidCredentials,
        Ldap_Other = 100
    };
}

class QnLdapManager : public Singleton<QnLdapManager> {
public:

    QnLdapManager();
    ~QnLdapManager();

    static QString errorMessage(Qn::LdapResult ldapResult);

    Qn::LdapResult fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings);
    Qn::LdapResult fetchUsers(QnLdapUsers &users);

    Qn::AuthResult realm(QString* realm) const;

    Qn::AuthResult authenticateWithDigest(const QString &login, const QString &ha1);
    Qn::LdapResult testSettings2(const QnLdapSettings& settings);

private:
    mutable QMap<QString, QString> m_realmCache;
    mutable QnMutex m_realmCacheMutex;
};

#endif // LDAP_MANAGER_H_
