#ifndef PING_SYSTEM_REST_HANDLER_H
#define PING_SYSTEM_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnPingSystemRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result);
};

#endif // PING_SYSTEM_REST_HANDLER_H
