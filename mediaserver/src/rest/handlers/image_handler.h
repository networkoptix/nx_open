#ifndef QN_REST_IMAGE_HANDLER_H
#define QN_REST_IMAGE_HANDLER_H

#include <QtCore/QByteArray>
#include "rest/server/request_handler.h"

class QnImageRestHandler: public QnRestRequestHandler
{
public:
    QnImageRestHandler();
    enum RoundMethod { IFrameBeforeTime, Precise, IFrameAfterTime };

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const;

private:
    int noVideoError(QByteArray& result, qint64 time);

private:
    bool m_detectAvailableOnly;
};

#endif // QN_REST_IMAGE_HANDLER_H
