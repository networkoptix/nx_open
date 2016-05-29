#ifndef QN_EXEC_SCRIPT_REST_HANDLER_H
#define QN_EXEC_SCRIPT_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"


class QnExecScript: public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    void afterExecute(const QString &path, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner) override;
};

#endif // QN_EXEC_SCRIPT_REST_HANDLER_H
