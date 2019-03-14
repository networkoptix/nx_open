#ifndef QN_STATISTICS_REST_HANDLER_H
#define QN_STATISTICS_REST_HANDLER_H

#include "rest/server/request_handler.h"
#include "rest/server/json_rest_handler.h"
#include <nx/vms/server/server_module_aware.h>

class QnGlobalMonitor;

class QnStatisticsRestHandler: public QnJsonRestHandler, public /*mixin*/ nx::vms::server::ServerModuleAware {
    Q_OBJECT
public:
    QnStatisticsRestHandler(QnMediaServerModule* serverModule);
    virtual ~QnStatisticsRestHandler();

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // QN_STATISTICS_REST_HANDLER_H
