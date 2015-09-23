/**********************************************************
* 16 jul 2014
* a.kolesnikov
***********************************************************/

#include "merge_ldap_users_rest_handler.h"

#include <common/common_module.h>
#include "common/user_permissions.h"
#include <utils/network/http/httptypes.h>

#include <core/resource/user_resource.h>
#include <ldap/ldap_manager.h>
#include "api/app_server_connection.h"

namespace {
    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }
}

int QnMergeLdapUsersRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) {
    QN_UNUSED(path, params, body);

    QnLdapManager *ldapManager = QnLdapManager::instance();
    QnLdapUsers ldapUsers;
    if (!ldapManager->fetchUsers(ldapUsers)) {
        result.setError(QnRestResult::CantProcessRequest, lit("Can't authenticate"));
        return nx_http::StatusCode::ok;
    }

    ec2::AbstractUserManagerPtr userManager = ec2Connection()->getUserManager();

    QnUserResourceList dbUsers;
    userManager->getUsersSync(QnUuid(), &dbUsers);

    auto findUser = [dbUsers](const QString& name) { for(QnUserResourcePtr user : dbUsers ) {if (name == user->getName()) return user; } return QnUserResourcePtr(); };
    for(QnLdapUser ldapUser : ldapUsers) {
        QString login = ldapUser.login.toLower();
        QnUserResourcePtr dbUser = findUser(login);
        if (!dbUser) {
            dbUser.reset(new QnUserResource());

            dbUser->setId(QnUuid::createUuid());
            dbUser->setTypeByName(lit("User"));

            dbUser->setName(login);

            // Without hash DbManager wont'd do anything
            dbUser->setHash(lit("-").toLatin1());
            dbUser->setEmail(ldapUser.email);
            dbUser->setAdmin(false);
            dbUser->setPermissions(Qn::GlobalLiveViewerPermissions);

            dbUser->setLdap(true);
            dbUser->setEnabled(false);
                
            QnUserResourceList newUsers;
            userManager->saveSync(dbUser, &newUsers);
        } else {
            // If regular user with same name already exists -> skip it
            if (!dbUser->isLdap())
                continue;

            if (dbUser->getEmail() != ldapUser.email) {
                dbUser->setEmail(ldapUser.email);

                QnUserResourceList newUsers;
                userManager->saveSync(dbUser, &newUsers);
            }
        }
    }

    return nx_http::StatusCode::ok;
}
