#ifndef UPDATE_REST_HANDLER_H
#define UPDATE_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"

class QnUpdateRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // UPDATE_REST_HANDLER_H
