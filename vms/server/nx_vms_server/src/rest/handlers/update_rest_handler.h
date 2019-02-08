#pragma once

#include "rest/server/json_rest_handler.h"

class QnServerUpdateTool;

class QnUpdateRestHandler : public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnUpdateRestHandler(QnServerUpdateTool* updateTool);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *processor) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*processor) override;

private:
    int handlePartialUpdate(const QString &updateId, const QByteArray &data, qint64 offset, QnJsonRestResult &result);
private:
    QnServerUpdateTool* m_updateTool = nullptr;
};
