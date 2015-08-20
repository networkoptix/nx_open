#include "test_ldap_rest_handler.h"

#include <utils/network/tcp_connection_priv.h>
#include "utils/common/ldap.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "ldap/ldap_manager.h"

int QnTestLdapSettingsHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) {
    QN_UNUSED(path, params);

    QnLdapSettings settings = QJson::deserialized(body, QnLdapSettings());

    auto ldapManager = QnLdapManager::instance();

    QnLdapUsers ldapUsers;
    if (!ldapManager->fetchUsers(ldapUsers, settings)) {
        result.setError(QnRestResult::CantProcessRequest, lit("Invalid ldap settings"));
        return nx_http::StatusCode::ok;
    }

    result.setReply(ldapUsers);
    return nx_http::StatusCode::ok;
}
