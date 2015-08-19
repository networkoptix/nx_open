#ifndef LDAP_MANAGER_H_
#define LDAP_MANAGER_H_

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <stdexcept>
#include <vector>
#include <map>

#include <utils/common/singleton.h>
#include <utils/common/ldap.h>


class QnLdapManagerPrivate;

class QnLdapManager : public Singleton<QnLdapManager> {
public:
    QnLdapManager();
    ~QnLdapManager();

    bool fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings);
    bool fetchUsers(QnLdapUsers &users);

    QString realm() const;

    bool authenticateWithDigest(const QString &login, const QString &ha1);
    bool testSettings(const QnLdapSettings& settings);
};

#endif // LDAP_MANAGER_H_
