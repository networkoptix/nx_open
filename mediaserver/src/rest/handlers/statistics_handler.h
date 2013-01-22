#ifndef QN_STATISTICS_HANDLER_H
#define QN_STATISTICS_HANDLER_H

#include "rest/server/request_handler.h"

class QnGlobalMonitor;

class QnStatisticsHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnStatisticsHandler();
    virtual ~QnStatisticsHandler();

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;

private:
    QnGlobalMonitor *m_monitor;
};

#endif // QN_STATISTICS_HANDLER_H
