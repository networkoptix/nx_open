/**********************************************************
* 16 jul 2014
* a.kolesnikov
***********************************************************/

#include "merge_ldap_users_rest_handler.h"

#include <common/common_module.h>
#include <nx/network/app_info.h>
#include <nx/network/http/http_types.h>

#include <core/resource/user_resource.h>
#include <ldap/ldap_manager.h>
#include "api/app_server_connection.h"

#include <nx_ec/ec_api.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx/vms/api/data/user_data.h>

#include <nx/utils/uuid.h>
#include "rest/server/rest_connection_processor.h"
#include <network/authenticate_helper.h>

int QnMergeLdapUsersRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams &/*params*/,
    const QByteArray& /*body*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnLdapUsers ldapUsers;
    Qn::LdapResult ldapResult = qnAuthHelper->ldapManager()->fetchUsers(ldapUsers);
    if (ldapResult != Qn::Ldap_NoError)
	{
        result.setError(QnRestResult::CantProcessRequest, QnLdapManager::errorMessage(ldapResult));
        return nx::network::http::StatusCode::ok;
    }

    ec2::AbstractUserManagerPtr userManager = owner->commonModule()->ec2Connection()->getUserManager(owner->accessRights());

    nx::vms::api::UserDataList dbUsers;
    userManager->getUsersSync(&dbUsers);

    const auto findUser =
        [dbUsers](const QString& name)
        {
            const auto iter = std::find_if(dbUsers.cbegin(), dbUsers.cend(),
                [name](const nx::vms::api::UserData& user) { return user.name == name; });

            if (iter != dbUsers.cend())
                return *iter;

            return nx::vms::api::UserData();
        };

    for (const auto& ldapUser: ldapUsers)
    {
        QString login = ldapUser.login.toLower();
        nx::vms::api::UserData dbUser = findUser(login);

        if (dbUser.id.isNull())
        {
            dbUser.id = QnUuid::createUuid();
            dbUser.name = login;
            dbUser.realm = nx::network::AppInfo::realm();

            // Without hash DbManager wont'd do anything
            dbUser.hash = lit("-").toLatin1();
            dbUser.email = ldapUser.email;
            dbUser.isAdmin = false;
            dbUser.permissions = GlobalPermission::liveViewerPermissions;

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
