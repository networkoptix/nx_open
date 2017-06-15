#pragma once

#include "rest/server/request_handler.h"

class QnImageRestHandler: public QnRestRequestHandler
{
    Q_OBJECT

public:
    virtual QStringList cameraIdUrlParamsForRequestForwarding() const override;

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override;

private:
    int noVideoError(QByteArray& result, qint64 time) const;

private:
    bool m_detectAvailableOnly;
};
