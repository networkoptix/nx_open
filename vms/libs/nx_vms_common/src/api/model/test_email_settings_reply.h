// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <utils/email/email_fwd.h>

struct NX_VMS_COMMON_API QnTestEmailSettingsReply
{
    QnTestEmailSettingsReply();

    nx::email::SmtpError errorCode;
    int smtpReplyCode;
    QString errorString;
};

#define QnTestEmailSettingsReply_Fields (errorCode)(smtpReplyCode)(errorString)

QN_FUSION_DECLARE_FUNCTIONS(QnTestEmailSettingsReply, (json)(ubjson), NX_VMS_COMMON_API)
