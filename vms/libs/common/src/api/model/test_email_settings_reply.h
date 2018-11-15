#pragma once

#include <QtCore/QMetaType>
#include <nx/fusion/model_functions_fwd.h>
#include <utils/email/email_fwd.h>

struct QnTestEmailSettingsReply
{
    QnTestEmailSettingsReply();

    SmtpError errorCode;
    SmtpReplyCode smtpReplyCode;
    QString errorString;
};

#define QnTestEmailSettingsReply_Fields (errorCode)(smtpReplyCode)(errorString)

QN_FUSION_DECLARE_FUNCTIONS(QnTestEmailSettingsReply, (json)(metatype))
