#ifndef QN_STORAGE_STATISTICS_HANDLER_H
#define QN_STORAGE_STATISTICS_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnPlatformMonitor;

class QnStorageSpaceHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnStorageSpaceHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, JsonResult &result) override;
    virtual QString description(TCPSocket *tcpSocket) const override;

    QnPlatformMonitor *m_monitor;
};

#endif // QN_STORAGE_STATISTICS_HANDLER_H
