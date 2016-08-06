#pragma once

#include <rest/server/json_rest_handler.h>

struct DetachFromCloudData;
class CloudConnectionManager;

class QnDetachFromCloudRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnDetachFromCloudRestHandler(const CloudConnectionManager& cloudConnectionManager);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
private:
    int execute(DetachFromCloudData passwordData, const QnUuid &userId, QnJsonRestResult &result);
private:
    const CloudConnectionManager& m_cloudConnectionManager;
};
