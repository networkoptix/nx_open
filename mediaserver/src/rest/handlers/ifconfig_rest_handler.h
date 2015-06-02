#ifndef IFCONFIG_REST_HANDLER_H
#define IFCONFIG_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"

class QnIfConfigRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // IFCONFIG_REST_HANDLER_H
