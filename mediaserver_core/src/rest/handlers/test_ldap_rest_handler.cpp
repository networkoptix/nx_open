#include "test_ldap_rest_handler.h"

#include <api/model/test_ldap_settings_reply.h>

#include <utils/network/tcp_connection_priv.h>
#include "utils/common/ldap.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "ldap/ldap_manager.h"
#include "nx_ec/data/api_ldap_data.h"

int QnTestLdapSettingsHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    Q_UNUSED(params);

    QnTestLdapSettingsReply reply;
    ec2::ApiLdapSettingsData apiData = QJson::deserialized(body, ec2::ApiLdapSettingsData());
    QnLdapSettings settings;
    fromApiToResource(apiData, settings);
    QnLdapManager *ldapManager = QnLdapManager::instance();

    if (!QnLdapManager::instance()->testSettings(settings)) {
        reply.errorCode = -1;
        reply.errorString = tr("Invalid ldap settings");
    }

    result.setReply(reply);
    return CODE_OK;
}
