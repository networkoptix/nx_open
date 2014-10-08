#ifndef QN_STATISTICS_REST_HANDLER_H
#define QN_STATISTICS_REST_HANDLER_H

#include "rest/server/request_handler.h"
#include "rest/server/json_rest_handler.h"

class QnGlobalMonitor;

class QnStatisticsRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnStatisticsRestHandler();
    virtual ~QnStatisticsRestHandler();

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    QnGlobalMonitor *m_monitor;
};


#endif // QN_STATISTICS_REST_HANDLER_H
