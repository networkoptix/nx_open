#include "test_ldap_rest_handler.h"

#include <network/tcp_connection_priv.h>
#include "utils/common/ldap.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "ldap/ldap_manager.h"
#include <network/authenticate_helper.h>

int QnTestLdapSettingsHandler::executePost(
    const QString &path,
    const QnRequestParams &params,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor*)
{
    QN_UNUSED(path, params);

    QnLdapSettings settings = QJson::deserialized(body, QnLdapSettings());

    QnLdapUsers ldapUsers;
    Qn::LdapResult ldapResult = qnAuthHelper->ldapManager()->fetchUsers(ldapUsers, settings);
    if (ldapResult != Qn::Ldap_NoError) {
        result.setError(QnRestResult::CantProcessRequest, QnLdapManager::errorMessage(ldapResult));
        return nx::network::http::StatusCode::ok;
    }

    result.setReply(ldapUsers);
    return nx::network::http::StatusCode::ok;
}
