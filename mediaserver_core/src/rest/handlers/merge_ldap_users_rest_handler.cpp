#include "merge_ldap_users_rest_handler.h"

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <network/universal_tcp_listener.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx/mediaserver/ldap_manager.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/uuid.h>
#include <rest/server/rest_connection_processor.h>

int QnMergeLdapUsersRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams &/*params*/,
    const QByteArray& /*body*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnLdapUsers ldapUsers;
    const auto ldapResult = QnUniversalTcpListener::authorizer(owner->owner())->ldapManager()
        ->fetchUsers(ldapUsers);
    if (ldapResult != nx::mediaserver::LdapResult::NoError)
	{
        result.setError(QnRestResult::CantProcessRequest, toString(ldapResult));
        return nx::network::http::StatusCode::ok;
    }

    ec2::AbstractUserManagerPtr userManager = owner->commonModule()->ec2Connection()->getUserManager(owner->accessRights());

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
            dbUser.permissions = Qn::GlobalLiveViewerPermissionSet;

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

    return nx::network::http::StatusCode::ok;
}
