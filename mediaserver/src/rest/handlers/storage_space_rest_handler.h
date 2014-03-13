#ifndef QN_STORAGE_SPACE_REST_HANDLER_H
#define QN_STORAGE_SPACE_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnPlatformMonitor;

class QnStorageSpaceRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnStorageSpaceRestHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
    virtual QString description() const override;

    QnPlatformMonitor *m_monitor;
};

#endif // QN_STORAGE_SPACE_REST_HANDLER_H
