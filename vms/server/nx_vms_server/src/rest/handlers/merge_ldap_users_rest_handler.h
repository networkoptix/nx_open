/**********************************************************
* 16 jul 2014
* a.kolesnikov
***********************************************************/

#pragma once

#include <rest/server/json_rest_handler.h>

class QnMergeLdapUsersRestHandler : public QnJsonRestHandler {
public:
    int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
};
