#include "test_email_rest_handler.h"

#include <api/model/test_email_settings_reply.h>

#include <utils/network/tcp_connection_priv.h>
#include "utils/common/email.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "business/email_manager_impl.h"
#include "nx_ec/data/api_email_data.h"

int QnTestEmailSettingsHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    QnTestEmailSettingsReply reply;
    ec2::ApiEmailSettingsData apiData = QJson::deserialized(body, ec2::ApiEmailSettingsData());
    QnEmail::Settings settings;
    fromApiToResource(apiData, settings);
    EmailManagerImpl emailManagerImpl;


    if (!emailManagerImpl.testConnection(settings)) {
        reply.errorCode = -1;
        reply.errorString = tr("Invalid email settings");
    }

    result.setReply(reply);
    return CODE_OK;
}
