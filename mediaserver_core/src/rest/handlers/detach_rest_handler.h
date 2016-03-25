#pragma once

#include <rest/server/json_rest_handler.h>

struct PasswordData;

class QnDetachFromSystemRestHandler: public QnJsonRestHandler 
{
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    int execute(PasswordData passwordData, QnJsonRestResult &result);
};
