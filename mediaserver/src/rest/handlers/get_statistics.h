#ifndef __REST_GET_STATISTICS_H__
#define __REST_GET_STATISTICS_H__

#include "rest/server/request_handler.h"

class QnPlatformMonitor;

class QnGetStatisticsHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnGetStatisticsHandler();
    virtual ~QnGetStatisticsHandler();

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;

private:
    QnPlatformMonitor *m_monitor;
};

#endif // __REST_GET_STATISTICS_H__
