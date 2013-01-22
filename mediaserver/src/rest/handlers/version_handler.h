#ifndef QN_VERSION_HANDLER_H
#define QN_VERSION_HANDLER_H

#include "rest/server/request_handler.h"

class QnVersionHandler: public QnRestRequestHandler
{
public:
protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
};

#endif // QN_VERSION_HANDLER_H
