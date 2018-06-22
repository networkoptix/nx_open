#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>

#include <nx/utils/singleton.h>
#include <utils/common/ldap.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_globals.h>
#include <common/common_module_aware.h>

namespace nx {
namespace mediaserver {

enum class LdapResult
{
    NoError,
    SizeLimit,
    InvalidCredentials,
    Other = 100
};

QString toString(LdapResult ldapResult);

class LdapManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:

    LdapManager(QnCommonModule* commonModule);
    ~LdapManager();

    LdapResult fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings);
    LdapResult fetchUsers(QnLdapUsers &users);

    Qn::AuthResult authenticate(const QString &login, const QString &password);

private slots:
    void clearCache();

private:
    mutable QMap<QString, QString> m_dnCache;
    mutable QnMutex m_cacheMutex;
};

} // namespace mediaserver
} // namespace nx
