#ifndef QN_EXEC_ACTION_HANDLER_H
#define QN_EXEC_ACTION_HANDLER_H

#include "rest/server/request_handler.h"

class QnGlobalMonitor;

class QnExecActionHandler: public QnRestRequestHandler {
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
};

#endif // QN_EXEC_ACTION_HANDLER_H
