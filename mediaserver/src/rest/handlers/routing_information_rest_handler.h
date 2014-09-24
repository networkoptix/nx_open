#ifndef ROUTING_INFORMATION_REST_HANDLER_H
#define ROUTING_INFORMATION_REST_HANDLER_H

#include <rest/server/request_handler.h>

class QnRoutingInformationRestHandler : public QnRestRequestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& resultContentType) override;
};

#endif // ROUTING_INFORMATION_REST_HANDLER_H
