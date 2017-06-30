#ifndef LDAP_MANAGER_H_
#define LDAP_MANAGER_H_

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>

#include <nx/utils/singleton.h>
#include <utils/common/ldap.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_globals.h>
#include <common/common_module_aware.h>

class QnLdapManagerPrivate;

namespace Qn {
    enum LdapResult {
        Ldap_NoError = 0,
        Ldap_SizeLimit,
        Ldap_InvalidCredentials,
        Ldap_Other = 100
    };
}

class QnLdapManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:

    QnLdapManager(QnCommonModule* commonModule);
    ~QnLdapManager();

    static QString errorMessage(Qn::LdapResult ldapResult);

    Qn::LdapResult fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings);
    Qn::LdapResult fetchUsers(QnLdapUsers &users);

    Qn::AuthResult authenticate(const QString &login, const QString &password);
private:
    Qn::AuthResult realm(QString* realm) const;
private slots:
    void clearCache();
private:
    mutable QMap<QString, QString> m_realmCache;
    mutable QMap<QString, QString> m_dnCache;
    mutable QnMutex m_cacheMutex;
};

#endif // LDAP_MANAGER_H_
