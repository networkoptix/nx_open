#ifndef ROUTING_INFORMATION_REST_HANDLER_H
#define ROUTING_INFORMATION_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnRoutingInformationRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *);
};

#endif // ROUTING_INFORMATION_REST_HANDLER_H
