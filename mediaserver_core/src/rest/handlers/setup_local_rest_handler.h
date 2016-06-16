#pragma once

#include <rest/server/json_rest_handler.h>

struct SetupLocalSystemData;

class QnSetupLocalSystemRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
private:
    int execute(SetupLocalSystemData data, const QnUuid &userId, QnJsonRestResult &result);
};
