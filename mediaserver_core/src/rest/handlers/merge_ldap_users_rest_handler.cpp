/**********************************************************
* 16 jul 2014
* a.kolesnikov
***********************************************************/

#include "merge_ldap_users_rest_handler.h"

#include <common/common_module.h>
#include <nx/network/http/httptypes.h>

#include <core/resource/user_resource.h>
#include <ldap/ldap_manager.h>
#include "api/app_server_connection.h"

#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/data/api_user_data.h>

#include <nx/utils/uuid.h>

namespace {
    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }
}

int QnMergeLdapUsersRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    QN_UNUSED(path, params, body);

    QnLdapManager *ldapManager = QnLdapManager::instance();
    QnLdapUsers ldapUsers;
    Qn::LdapResult ldapResult = ldapManager->fetchUsers(ldapUsers);
    if (ldapResult != Qn::Ldap_NoError)
	{
        result.setError(QnRestResult::CantProcessRequest, QnLdapManager::errorMessage(ldapResult));
        return nx_http::StatusCode::ok;
    }

    ec2::AbstractUserManagerPtr userManager = ec2Connection()->getUserManager();

    ec2::ApiUserDataList dbUsers;
    userManager->getUsersSync(&dbUsers);

    auto findUser = [dbUsers](const QString& name)
    {
        auto iter = std::find_if(dbUsers.cbegin(), dbUsers.cend(), [name](const ec2::ApiUserData& user)
        {
            return user.name == name;
        });
        if (iter != dbUsers.cend())
            return *iter;
        return ec2::ApiUserData();
    };

    for(const QnLdapUser &ldapUser : ldapUsers)
    {
        QString login = ldapUser.login.toLower();
        ec2::ApiUserData dbUser = findUser(login);
        if (dbUser.id.isNull())
        {
            dbUser.id = QnUuid::createUuid();
            dbUser.typeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kUserTypeId);
            dbUser.name = login;

            // Without hash DbManager wont'd do anything
            dbUser.hash = lit("-").toLatin1();
            dbUser.email = ldapUser.email;
            dbUser.isAdmin = false;
            dbUser.permissions = Qn::GlobalLiveViewerPermissions;

            dbUser.isLdap = true;
            dbUser.isEnabled = false;

            userManager->saveSync(dbUser);
        }
        else
        {
            // If regular user with same name already exists -> skip it
            if (!dbUser.isLdap)
                continue;

            if (dbUser.email != ldapUser.email)
            {
                dbUser.email = ldapUser.email;
                userManager->saveSync(dbUser);
            }
        }
    }

    return nx_http::StatusCode::ok;
}
