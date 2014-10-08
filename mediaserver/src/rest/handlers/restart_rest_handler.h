#ifndef RESTART_REST_HANDLER_H
#define RESTART_REST_HANDLER_H

#include "rest/server/request_handler.h"

class QnRestartRestHandler : public QnRestRequestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, 
                            QByteArray& resultContentType, const QnRestConnectionProcessor*) override;
};

#endif // RESTART_REST_HANDLER_H
