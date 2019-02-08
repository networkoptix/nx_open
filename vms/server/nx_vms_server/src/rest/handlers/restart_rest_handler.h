#ifndef RESTART_REST_HANDLER_H
#define RESTART_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"

class QnRestartRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *) override;

    virtual int executePost(const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    virtual void afterExecute(const QString &path, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner) override;

};

#endif // RESTART_REST_HANDLER_H
