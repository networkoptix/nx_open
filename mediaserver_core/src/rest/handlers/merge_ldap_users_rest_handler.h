/**********************************************************
* 16 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef MERGE_LDAP_USERS_REST_HANDLER_H
#define MERGE_LDAP_USERS_REST_HANDLER_H

#include <QtCore/QByteArray>

#include "rest/server/request_handler.h"


class QnMergeLdapUsersRestHandler
:
    public QnRestRequestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, 
                            QByteArray& contentType, const QnRestConnectionProcessor*) override;
};

#endif  // MERGE_LDAP_USERS_REST_HANDLER_H
