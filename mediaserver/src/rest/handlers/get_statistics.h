#ifndef QN_GET_STATISTICS_HANDLER_H
#define QN_GET_STATISTICS_HANDLER_H

#include "rest/server/request_handler.h"

class QnGlobalMonitor;

class QnGetStatisticsHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnGetStatisticsHandler();
    virtual ~QnGetStatisticsHandler();

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;

private:
    QnGlobalMonitor *m_monitor;
};

#endif // QN_GET_STATISTICS_HANDLER_H
