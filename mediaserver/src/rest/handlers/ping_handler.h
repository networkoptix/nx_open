#ifndef QN_REST_PING__HANDLER_H
#define QN_REST_PING__HANDLER_H

#include "rest/server/request_handler.h"

// TODO: rename to QnTimeHandler, there are no other handlers with 'Get' prefix.
class QnRestPingHandler: public QnRestRequestHandler
{
public:
    QnRestPingHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, QByteArray& contentEncoding);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType, QByteArray& contentEncoding);
    virtual QString description(TCPSocket* tcpSocket) const;
};

#endif // QN_REST_PING__HANDLER_H
