#include "test_email_settings_reply.h"

#include <utils/email/email.h>

QnTestEmailSettingsReply::QnTestEmailSettingsReply():
    errorCode(SmtpError::Success),
    smtpReplyCode(SmtpReplyCode::NoReply),
    errorString()
{

}
