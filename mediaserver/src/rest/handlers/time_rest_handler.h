#ifndef QN_TIME_REST_HANDLER_H
#define QN_TIME_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnTimeRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
};

#endif // QN_TIME_REST_HANDLER_H
