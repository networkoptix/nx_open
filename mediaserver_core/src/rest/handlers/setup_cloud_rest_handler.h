#pragma once

#include <rest/server/json_rest_handler.h>

class CloudConnectionManager;
struct SetupRemoveSystemData;

class QnSetupCloudSystemRestHandler: public QnJsonRestHandler 
{
    Q_OBJECT
public:
    QnSetupCloudSystemRestHandler(const CloudConnectionManager& cloudConnectionManager);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
private:
    const CloudConnectionManager& m_cloudConnectionManager;

    int execute(SetupRemoveSystemData data, const QnUuid &userId, QnJsonRestResult &result);
};
