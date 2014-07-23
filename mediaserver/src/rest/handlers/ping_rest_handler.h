#ifndef QN_PING_REST_HANDLER_H
#define QN_PING_REST_HANDLER_H

#include "rest/server/request_handler.h"


class QnPingRestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType);
};

#endif // QN_PING_REST_HANDLER_H
