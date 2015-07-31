#ifndef TEST_LDAP_SETTINGS_REPLY_H
#define TEST_LDAP_SETTINGS_REPLY_H

#include <QtCore/QMetaType>
#include <utils/common/model_functions_fwd.h>

struct QnTestLdapSettingsReply
{

    QnTestLdapSettingsReply(): errorCode(0) { }

    int errorCode;
    QString errorString;
};

#define QnTestLdapSettingsReply_Fields (errorCode)(errorString)
    QN_FUSION_DECLARE_FUNCTIONS(QnTestLdapSettingsReply, (json)(metatype))


#endif // TEST_LDAP_SETTINGS_REPLY_H
