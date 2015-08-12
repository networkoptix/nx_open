#ifndef MERGE_LDAP_USERS_REPLY_H
#define MERGE_LDAP_USERS_REPLY_H

#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>


struct QnMergeLdapUsersReply
{
    int errorCode;
    QString errorMessage;

    QnMergeLdapUsersReply()
        :errorCode( 0 )
    {
    }
};

#define QnMergeLdapUsersReply_Fields (errorCode)(errorMessage)

QN_FUSION_DECLARE_FUNCTIONS(QnMergeLdapUsersReply, (json)(metatype))

#endif // MERGE_LDAP_USERS_REPLY_H
