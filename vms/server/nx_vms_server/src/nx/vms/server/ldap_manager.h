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
namespace vms::server {

enum class LdapResult
{
    NoError,
    SizeLimit,
    InvalidCredentials,
    Other = 100
};

QString toString(LdapResult ldapResult);

class AbstractLdapManager
{
public:
    virtual ~AbstractLdapManager() = default;
    virtual Qn::AuthResult authenticate(const QString& login, const QString& password) = 0;
    virtual LdapResult fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings) = 0;
    virtual LdapResult fetchUsers(QnLdapUsers &users) = 0;
};

class LdapManager: public QObject, public AbstractLdapManager, public QnCommonModuleAware
{
    Q_OBJECT
public:
    LdapManager(QnCommonModule* commonModule);
    virtual ~LdapManager() override;

    virtual LdapResult fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings) override;
    virtual LdapResult fetchUsers(QnLdapUsers &users) override;

    virtual Qn::AuthResult authenticate(const QString& login, const QString& password) override;

private slots:
    void clearCache();

private:
    mutable QMap<QString, QString> m_dnCache;
    mutable QnMutex m_cacheMutex;
};

} // namespace vms::server
} // namespace nx
