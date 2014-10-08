#ifndef QN_STORAGE_SPACE_REST_HANDLER_H
#define QN_STORAGE_SPACE_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnPlatformMonitor;

class QnStorageSpaceRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnStorageSpaceRestHandler();

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    QnPlatformMonitor *m_monitor;
};

#endif // QN_STORAGE_SPACE_REST_HANDLER_H
