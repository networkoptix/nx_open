#ifndef LDAP_MANAGER_H_
#define LDAP_MANAGER_H_

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <stdexcept>
#include <vector>
#include <map>

#include <utils/common/singleton.h>
#include <utils/common/ldap.h>

struct QnLdapUser {
    QString dn;
    QString login;
    QString fullName;
    QString email;
};

typedef QList<QnLdapUser> QnLdapUsers;

class QnLdapException : std::exception {
public:
    QnLdapException(const char *msg)
        : _msg(msg) {
    }

    const char* what() const
#ifdef __GNUC__
    noexcept (true)
#endif
 {
        return _msg.c_str();
    }
private:
    std::string _msg;
};

class QnLdapManagerPrivate;

class QnLdapManager : public Singleton<QnLdapManager> {
public:
    QnLdapManager();
    ~QnLdapManager();

    QnLdapUsers fetchUsers();
    QnLdapUsers users();

    QString realm() const;

    bool authenticateWithDigest(const QString &login, const QString &ha1);

    static bool testSettings(const QnLdapSettings& settings);

private:
	Q_DECLARE_PRIVATE(QnLdapManager);

	QnLdapManagerPrivate *d_ptr;
};

#endif // LDAP_MANAGER_H_
