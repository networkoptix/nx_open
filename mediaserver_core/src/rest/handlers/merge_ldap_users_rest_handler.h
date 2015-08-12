/**********************************************************
* 16 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef MERGE_LDAP_USERS_REST_HANDLER_H
#define MERGE_LDAP_USERS_REST_HANDLER_H

#include <QtCore/QByteArray>

#include <rest/server/json_rest_handler.h>


class QnMergeLdapUsersRestHandler
:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif  // MERGE_LDAP_USERS_REST_HANDLER_H
