#ifndef QN_TEST_LDAP_HANDLER_H
#define QN_TEST_LDAP_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnTestLdapSettingsHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // QN_TEST_LDAP_HANDLER_H
