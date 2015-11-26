#ifndef LDAP_MANAGER_H_
#define LDAP_MANAGER_H_

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>

#include <nx/utils/singleton.h>
#include <utils/common/ldap.h>
#include <nx/utils/thread/mutex.h>
#include "common/common_globals.h"


class QnLdapManagerPrivate;

class QnLdapManager : public Singleton<QnLdapManager> {
public:

    QnLdapManager();
    ~QnLdapManager();

    bool fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings);
    bool fetchUsers(QnLdapUsers &users);

    Qn::AuthResult realm(QString* realm) const;

    Qn::AuthResult authenticateWithDigest(const QString &login, const QString &ha1);
    bool testSettings(const QnLdapSettings& settings);

private:
    mutable QMap<QString, QString> m_realmCache;
    mutable QnMutex m_realmCacheMutex;
};

#endif // LDAP_MANAGER_H_
