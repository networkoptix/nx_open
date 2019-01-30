#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

class QnCancelUpdateRestHandler:
    public QnFusionRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnCancelUpdateRestHandler(QnMediaServerModule* serverModule);

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;
};
