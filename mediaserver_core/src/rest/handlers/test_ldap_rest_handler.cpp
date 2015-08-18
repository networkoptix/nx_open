#include "test_ldap_rest_handler.h"

#include <utils/network/tcp_connection_priv.h>
#include "utils/common/ldap.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "ldap/ldap_manager.h"
#include "nx_ec/data/api_ldap_data.h"

int QnTestLdapSettingsHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) {
    QN_UNUSED(path, params);

    ec2::ApiLdapSettingsData apiData = QJson::deserialized(body, ec2::ApiLdapSettingsData());
    QnLdapSettings settings;
    fromApiToResource(apiData, settings);

    if (!QnLdapManager::instance()->testSettings(settings))
        result.setError(QnRestResult::CantProcessRequest, lit("Invalid ldap settings"));

    return nx_http::StatusCode::ok;
}
