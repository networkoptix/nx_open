#ifndef LDAP_MANAGER_H_
#define LDAP_MANAGER_H_

#include <QtCore/QString>
#include <stdexcept>
#include <vector>
#include <map>

struct LdapUser {
    QString dn;
    QString login;
    QString fullName;
};

typedef std::map<QString, LdapUser> LdapUsers;

class LdapException : std::exception {
public:
    LdapException(const char *msg)
        : _msg(msg) {
    }

    const char* what() const {
        return _msg.c_str();
    }
private:
    std::string _msg;
};

class QnLdapManagerPrivate;

class QnLdapManager {
public:
    QnLdapManager(const QString &host, int port, const QString &bindDn, const QString &password, const QString &searchBase);
    ~QnLdapManager();

    void bind();

    void fetchUsers();
    QStringList users();

    bool verifyCredentials(const QString &login, const QString &password);

private:
	Q_DECLARE_PRIVATE(QnLdapManager);

	QnLdapManagerPrivate *d_ptr;
};

#endif // LDAP_MANAGER_H_