#ifndef QN_GETTIME_HANDLER_H
#define QN_GETTIME_HANDLER_H

#include "rest/server/request_handler.h"

class QnGetTimeHandler: public QnRestRequestHandler
{
public:
    QnGetTimeHandler();
protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
};

#endif // QN_GETTIME_HANDLER_H
