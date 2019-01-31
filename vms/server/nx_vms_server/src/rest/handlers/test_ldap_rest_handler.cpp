#include "test_ldap_rest_handler.h"

#include <network/tcp_connection_priv.h>
#include <network/universal_tcp_listener.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/server/ldap_manager.h>
#include <utils/common/ldap.h>

int QnTestLdapSettingsHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{

    QnLdapSettings settings = QJson::deserialized(body, QnLdapSettings());

    QnLdapUsers ldapUsers;
    const auto ldapResult = QnUniversalTcpListener::authenticator(owner->owner())->ldapManager()
        ->fetchUsers(ldapUsers, settings);
    if (ldapResult != nx::vms::server::LdapResult::NoError)
    {
        result.setError(QnRestResult::CantProcessRequest, toString(ldapResult));
        return nx::network::http::StatusCode::ok;
    }

    result.setReply(ldapUsers);
    return nx::network::http::StatusCode::ok;
}
