#include "test_ldap_rest_handler.h"

#include <network/tcp_connection_priv.h>
#include <network/universal_tcp_listener.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/server/ldap_manager.h>
#include <utils/common/ldap.h>

namespace nx::vms::server::rest {

nx::network::rest::Response TestLdapSettingsHandler::executePost(
    const nx::network::rest::Request& request)
{
    QnLdapUsers ldapUsers;
    const auto ldapResult = QnUniversalTcpListener::authenticator(request.owner->owner())
        ->ldapManager()->fetchUsers(ldapUsers, request.parseContentOrThrow<QnLdapSettings>());

    if (ldapResult != nx::vms::server::LdapResult::NoError)
    {
        return nx::network::rest::Response::error(
            nx::network::http::StatusCode::ok,
            QnRestResult::CantProcessRequest, toString(ldapResult));
    }

    return nx::network::rest::Response::reply(ldapUsers);
}

} // namespace nx::vms::server::rest
