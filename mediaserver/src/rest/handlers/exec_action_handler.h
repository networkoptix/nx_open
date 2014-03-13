#ifndef QN_EXEC_ACTION_HANDLER_H
#define QN_EXEC_ACTION_HANDLER_H

#include "rest/server/request_handler.h"

class QnGlobalMonitor;

// TODO: #Elric rename business action rest handler

class QnExecActionHandler: public QnRestRequestHandler {
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const;
};

#endif // QN_EXEC_ACTION_HANDLER_H
