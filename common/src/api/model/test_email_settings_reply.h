#ifndef TEST_EMAIL_SETTINGS_REPLY_H
#define TEST_EMAIL_SETTINGS_REPLY_H

#include <QtCore/QMetaType>
#include <utils/common/model_functions_fwd.h>

struct QnTestEmailSettingsReply
{

    QnTestEmailSettingsReply(): errorCode(0) { }

    int errorCode;
    QString errorString;
};

#define QnTestEmailSettingsReply_Fields (errorCode)(errorString)
    QN_FUSION_DECLARE_FUNCTIONS(QnTestEmailSettingsReply, (json)(metatype))


#endif // TEST_EMAIL_SETTINGS_REPLY_H
