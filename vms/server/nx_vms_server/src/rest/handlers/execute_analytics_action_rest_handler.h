#pragma once

#include <rest/server/json_rest_handler.h>
#include "nx/sdk/analytics/device_agent.h"
#include "nx/vms/api/analytics/engine_manifest.h"
#include "api/model/analytics_actions.h"
#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/sdk_support/loggers.h>

class QnExecuteAnalyticsActionRestHandler:
    public QnJsonRestHandler,
    public nx::mediaserver::ServerModuleAware
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
        nx::sdk::analytics::Engine* plugin,
        const AnalyticsAction& actionData);

    std::unique_ptr<nx::mediaserver::sdk_support::AbstractManifestLogger> makeLogger(
        nx::mediaserver::resource::AnalyticsEngineResourcePtr engineResource) const;
};
