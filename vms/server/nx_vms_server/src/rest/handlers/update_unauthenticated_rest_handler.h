#pragma once

#include "rest/server/json_rest_handler.h"

class QnServerUpdateTool;

class QnUpdateUnauthenticatedRestHandler: public QnJsonRestHandler
{
public:
    QnUpdateUnauthenticatedRestHandler(QnServerUpdateTool* updateTool);

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* processor) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* processor) override;
private:
    QnServerUpdateTool* m_updateTool = nullptr;
};
