#ifndef SETTIME_REST_HANDLER_H
#define SETTIME_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"

class QnSetTimeRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // SETTIME_REST_HANDLER_H
