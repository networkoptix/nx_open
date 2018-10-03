#include "test_email_rest_handler.h"

#include <api/model/test_email_settings_reply.h>

#include <nx/email/email_manager_impl.h>
#include <network/tcp_connection_priv.h>
#include <utils/email/email.h>

#include "nx_ec/data/api_conversion_functions.h"
#include <nx/vms/api/data/email_settings_data.h>

#include <rest/server/rest_connection_processor.h>

int QnTestEmailSettingsHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnTestEmailSettingsReply reply;
    const auto apiData = QJson::deserialized<nx::vms::api::EmailSettingsData>(body);
    QnEmailSettings settings;
    ec2::fromApiToResource(apiData, settings);
    EmailManagerImpl emailManagerImpl(owner->commonModule());

    SmtpOperationResult smtpResult = emailManagerImpl.testConnection(settings);

    if (!smtpResult)
    {
        reply.errorCode = smtpResult.error;
        reply.smtpReplyCode = smtpResult.lastCode;
        reply.errorString = lit("Invalid email settings");
    }
    else
    {
        reply.errorCode = SmtpError::Success;
        reply.smtpReplyCode = SmtpReplyCode::AuthSuccessful;
    }

    result.setReply(reply);
    return nx::network::http::StatusCode::ok;
}
