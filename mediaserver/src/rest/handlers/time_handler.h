#ifndef QN_TIME_HANDLER_H
#define QN_TIME_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnTimeHandler: public QnJsonRestHandler {
public:
    QnTimeHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
    virtual QString description() const override;
};

#endif // QN_TIME_HANDLER_H
