#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/mediaserver/server_module_aware.h>

class QnDebugEventsRestHandler: public QnJsonRestHandler, public nx::mediaserver::ServerModuleAware
{
    Q_OBJECT
public:
    QnDebugEventsRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;

private:
    int testNetworkIssue(
        const QnRequestParams & params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner);
    int testCameraDisconnected(
        const QnRequestParams & params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner);

};
