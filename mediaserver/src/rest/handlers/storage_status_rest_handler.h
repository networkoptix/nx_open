#ifndef QN_STORAGE_STATUS_REST_HANDLER_H
#define QN_STORAGE_STATUS_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnStorageStatusRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:

protected:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
    virtual QString description() const override;
};

#endif // QN_STORAGE_STATUS_REST_HANDLER_H
