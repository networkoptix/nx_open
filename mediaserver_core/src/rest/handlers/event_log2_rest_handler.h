#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/mediaserver/server_module_aware.h>

class QnEventLog2RestHandler:
    public QnFusionRestHandler,
    public nx::mediaserver::ServerModuleAware
{
public:
    QnEventLog2RestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* owner) override;
};
