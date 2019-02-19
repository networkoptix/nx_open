#pragma once

#include <rest/server/request_handler.h>

class QnServerControlHandler: public QnRestRequestHandler
{
    Q_OBJECT

public:
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
};
