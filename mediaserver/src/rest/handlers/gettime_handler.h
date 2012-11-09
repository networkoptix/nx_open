#ifndef QN_TIME_HANDLER_H
#define QN_TIME_HANDLER_H

#include "rest/server/request_handler.h"

// TODO: rename to QnTimeHandler, there are no other handlers with 'Get' prefix.
class QnGetTimeHandler: public QnRestRequestHandler
{
public:
    QnGetTimeHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
};

#endif // QN_TIME_HANDLER_H
