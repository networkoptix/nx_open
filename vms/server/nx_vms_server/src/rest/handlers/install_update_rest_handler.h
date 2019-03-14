#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/utils/move_only_func.h>

class QnInstallUpdateRestHandler:
    public QnFusionRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnInstallUpdateRestHandler(QnMediaServerModule* serverModule,
        nx::utils::MoveOnlyFunc<void()> onTriggeredCallback);

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;

    virtual void afterExecute(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QnRestConnectionProcessor* owner) override;

private:
    nx::utils::MoveOnlyFunc<void()> m_onTriggeredCallback;
};
