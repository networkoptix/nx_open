#pragma once

#include <rest/server/json_rest_handler.h>
#include "nx/sdk/analytics/i_device_agent.h"
#include "nx/vms/api/analytics/engine_manifest.h"
#include "api/model/analytics_actions.h"
#include <nx/vms/server/server_module_aware.h>

#include <api/model/analytics_actions.h>
#include <nx/analytics/db/abstract_storage.h>
#include <transcoding/filters/abstract_image_filter.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/vms/api/analytics/descriptors.h>

struct ExtendedAnalyticsActionData
{
    AnalyticsAction action;
    nx::vms::server::resource::AnalyticsEngineResourcePtr engine;
    nx::vms::api::analytics::ActionTypeDescriptor actionTypeDescriptor;

    std::optional<nx::analytics::db::ObjectTrackEx> objectTrack;
    std::optional<nx::analytics::db::ObjectPosition> bestShotObjectPosition;
    CLVideoDecoderOutputPtr bestShotVideoFrame;

    QByteArray bestShotImageData;
    QString bestShotImageDataFormat;
};

class QnExecuteAnalyticsActionRestHandler:
    public QnJsonRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnExecuteAnalyticsActionRestHandler(QnMediaServerModule* serverModule);

    virtual QStringList cameraIdUrlParams() const override;

    virtual int executePost(
        const QString &path,
        const QnRequestParams &params,
        const QByteArray &body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner) override;

private:
    std::optional<AnalyticsAction> deserializeActionData(const QByteArray& requestBody) const;

    QString checkInputParameters(const AnalyticsAction& body) const;

    nx::vms::server::resource::AnalyticsEngineResourcePtr engineForAction(
        const AnalyticsAction& actionData) const;

    std::optional<nx::vms::api::analytics::ActionTypeDescriptor> actionDescriptor(
        const AnalyticsAction& actionData) const;

    std::optional<ExtendedAnalyticsActionData> makeExtendedAnalyticsActionData(
        const AnalyticsAction& action,
        const nx::vms::server::resource::AnalyticsEngineResourcePtr engineResource,
        const nx::vms::api::analytics::ActionTypeDescriptor& actionTypeDescriptor);

    enum class ExecuteActionStatus { noError, internalError, pluginError };

    /** @return Http status code. */
    int doExecuteAction(QnJsonRestResult& result,
        nx::vms::server::resource::AnalyticsEngineResourcePtr engineResource,
        const ExtendedAnalyticsActionData& extendedActionData);

    ExecuteActionStatus executeAction(
        const ExtendedAnalyticsActionData& actionData,
        AnalyticsActionResult* outActionResult,
        QString* outErrorMessage);

    std::optional<nx::analytics::db::ObjectTrackEx> fetchObjectTrack(
        const QnUuid& objectTrackId,
        bool needFullTrack);

    CLVideoDecoderOutputPtr imageByTimestamp(
        const QnUuid& deviceId,
        const int64_t timestampUs,
        nx::vms::api::StreamIndex streamIndex);

    std::optional<nx::analytics::db::LookupResult> makeDatabaseRequest(
        const nx::analytics::db::Filter& filter);

};
