#pragma once

#include <rest/server/json_rest_handler.h>
#include "nx/sdk/metadata/camera_manager.h"
#include "nx/api/analytics/driver_manifest.h"
#include "api/model/analytics_actions.h"

class QnExecuteAnalyticsActionRestHandler: public QnJsonRestHandler
{
public:
    virtual int executePost(
        const QString &path,
        const QnRequestParams &params,
        const QByteArray &body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner) override;

private:
    QString executeAction(
        AnalyticsActionResult* outActionResult,
        nx::sdk::metadata::Plugin* plugin,
        const AnalyticsAction& actionData);
};
