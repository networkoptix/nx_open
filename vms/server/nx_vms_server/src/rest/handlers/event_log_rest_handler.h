#pragma once

#include <rest/server/request_handler.h>
#include <nx/vms/server/server_module_aware.h>

class QnEventLogRestHandler:
    public QnRestRequestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    QnEventLogRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path, const QnRequestParamList& params, const QByteArray& body,
        const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override;
};
