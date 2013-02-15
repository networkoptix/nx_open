#ifndef QN_LOG__HANDLER_H
#define QN_LOG__HANDLER_H

#include "rest/server/request_handler.h"

// TODO: rename to QnTimeHandler, there are no other handlers with 'Get' prefix.
class QnRestLogHandler: public QnRestRequestHandler
{
public:
    QnRestLogHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
};

#endif // QN_LOG__HANDLER_H
