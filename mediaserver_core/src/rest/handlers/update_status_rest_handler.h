#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/mediaserver/server_module_aware.h>

class QnUpdateStatusRestHandler:
    public QnFusionRestHandler, public nx::mediaserver::ServerModuleAware
{
public:
    QnUpdateStatusRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor*processor) override;
};
