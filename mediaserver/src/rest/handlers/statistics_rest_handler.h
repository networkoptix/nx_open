#ifndef QN_STATISTICS_REST_HANDLER_H
#define QN_STATISTICS_REST_HANDLER_H

#include "rest/server/request_handler.h"

class QnGlobalMonitor;

class QnStatisticsRestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnStatisticsRestHandler();
    virtual ~QnStatisticsRestHandler();

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType);
private:
    QnGlobalMonitor *m_monitor;
};

#endif // QN_STATISTICS_REST_HANDLER_H
