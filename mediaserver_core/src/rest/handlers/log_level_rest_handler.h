#pragma once

#include <rest/server/json_rest_handler.h>

class QnLogLevelRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *processor);
};
