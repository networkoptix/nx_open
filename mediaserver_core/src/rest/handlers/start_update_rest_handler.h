#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/mediaserver/server_module_aware.h>

class QnStartUpdateRestHandler:
    public QnFusionRestHandler,
    public nx::mediaserver::ServerModuleAware
{
public:
    QnStartUpdateRestHandler(QnMediaServerModule* serverModule);

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;
};
