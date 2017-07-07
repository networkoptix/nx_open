#pragma once

#include <rest/server/request_handler.h>

class QnEventLogRestHandler: public QnRestRequestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path, const QnRequestParamList& params, const QByteArray& body,
        const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override;
};
