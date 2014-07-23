#ifndef QN_EXEC_ACTION_HANDLER_H
#define QN_EXEC_ACTION_HANDLER_H

#include "rest/server/request_handler.h"

class QnGlobalMonitor;

class QnBusinessActionRestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType);
};

#endif // QN_EXEC_ACTION_HANDLER_H
