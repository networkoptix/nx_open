#ifndef QN_STORAGE_STATISTICS_HANDLER_H
#define QN_STORAGE_STATISTICS_HANDLER_H

#include "rest/server/request_handler.h"

class QnPlatformMonitor;

class QnStorageSpaceHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnStorageSpaceHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) override;
    virtual int executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) override;
    virtual QString description(TCPSocket *tcpSocket) const override;

    QnPlatformMonitor *m_monitor;
};

#endif // QN_STORAGE_STATISTICS_HANDLER_H
