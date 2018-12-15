#pragma once

#include <rest/server/json_rest_handler.h>
#include "nx/sdk/analytics/i_device_agent.h"
#include "nx/vms/api/analytics/engine_manifest.h"
#include "api/model/analytics_actions.h"
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/sdk_support/loggers.h>

class QnExecuteAnalyticsActionRestHandler:
    public QnJsonRestHandler,
    public nx::vms::server::ServerModuleAware
{
public:
    QnExecuteAnalyticsActionRestHandler(QnMediaServerModule* serverModule);

    virtual int executePost(
        const QString &path,
        const QnRequestParams &params,
        const QByteArray &body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner) override;

private:
    QString executeAction(
        AnalyticsActionResult* outActionResult,
        nx::sdk::analytics::IEngine* plugin,
        const AnalyticsAction& actionData);

    std::unique_ptr<nx::vms::server::sdk_support::AbstractManifestLogger> makeLogger(
        nx::vms::server::resource::AnalyticsEngineResourcePtr engineResource) const;
};
