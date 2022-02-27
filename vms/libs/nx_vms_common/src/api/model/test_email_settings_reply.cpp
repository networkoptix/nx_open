// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_email_settings_reply.h"

#include <utils/email/email.h>

QnTestEmailSettingsReply::QnTestEmailSettingsReply():
    errorCode(nx::email::SmtpError::success),
    smtpReplyCode(SmtpReplyCode::NoReply),
    errorString()
{

}
