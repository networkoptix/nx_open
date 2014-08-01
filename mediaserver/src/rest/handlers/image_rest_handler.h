#ifndef QN_IMAGE_REST_HANDLER_H
#define QN_IMAGE_REST_HANDLER_H

#include <QtCore/QByteArray>
#include "rest/server/request_handler.h"

class QnImageRestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    enum RoundMethod { IFrameBeforeTime, Precise, IFrameAfterTime };

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType);

private:
    int noVideoError(QByteArray& result, qint64 time);

private:
    bool m_detectAvailableOnly;
};

#endif // QN_IMAGE_REST_HANDLER_H
