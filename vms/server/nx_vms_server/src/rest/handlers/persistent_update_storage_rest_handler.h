#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

class QnPersistentUpdateStorageRestHandler :
    public QnFusionRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnPersistentUpdateStorageRestHandler(QnMediaServerModule* serverModule);

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* processor) override;
};
