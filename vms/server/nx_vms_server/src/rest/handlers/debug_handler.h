#pragma once

#include <rest/server/request_handler.h>

class QnDebugHandler: public QnRestRequestHandler
{
    Q_OBJECT

protected:
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& responseMessageBody,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& requestBody,
        const QByteArray& srcBodyContentType,
        QByteArray& responseMessageBody,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override;

    virtual void afterExecute(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QnRestConnectionProcessor* owner) override;
};
