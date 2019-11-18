#include "execute_analytics_action_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <media_server/media_server_module.h>
#include <nx/vms/server/analytics/manager.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/uuid.h>
#include <nx/vms_server_plugins/utils/uuid.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/conversion_utils.h>
#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/server/analytics/wrappers/engine.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/helpers/object_track_info.h>
#include <nx/sdk/uuid.h>

#include <nx/analytics/action_type_descriptor_manager.h>

#include <camera/get_image_helper.h>
#include <analytics/db/abstract_storage.h>

#include <plugins/settings.h>

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::analytics::db;
using namespace nx::vms::server;
using namespace nx::vms::server::analytics;
using namespace nx::vms_server_plugins::utils;

template<typename T>
using ResultHolder = nx::vms::server::sdk_support::ResultHolder<T>;

namespace {

Ptr<IObjectTrackInfo> makeObjectTrackInfo(
    const ExtendedAnalyticsActionData& actionData)
{
    using namespace nx::vms::server::sdk_support;
    const auto objectTrackInfo = makePtr<ObjectTrackInfo>();

    if (actionData.objectTrack)
    {

        if (actionData.bestShotObjectPosition)
        {
            const auto bestShotObjectMetadata = createTimestampedObjectMetadata(
                *actionData.objectTrack,
                *actionData.bestShotObjectPosition);

            if (!bestShotObjectMetadata)
                return nullptr;

            objectTrackInfo->setBestShotObjectMetadata(bestShotObjectMetadata.get());
        }
    }

    if (actionData.bestShotVideoFrame)
    {
        const Ptr<IUncompressedVideoFrame> bestShotVideoFrame = createUncompressedVideoFrame(
            actionData.bestShotVideoFrame,
            actionData.actionTypeDescriptor.requirements.bestShotVideoFramePixelFormat);

        if (!bestShotVideoFrame)
            return nullptr;

        objectTrackInfo->setBestShotVideoFrame(bestShotVideoFrame.get());
    }

    return objectTrackInfo;
}

class Action: public RefCountable<IAction>
{
public:
    Action(const ExtendedAnalyticsActionData& actionData):
        m_actionId(actionData.action.actionId.toStdString()),
        m_objectTrackId(fromQnUuidToSdkUuid(actionData.action.objectTrackId)),
        m_deviceId(fromQnUuidToSdkUuid(actionData.action.deviceId)),
        m_timestampUs(actionData.action.timestampUs),
        m_objectTrackInfo(makeObjectTrackInfo(actionData)),
        m_params(sdk_support::toSdkStringMap(actionData.action.params))
    {
    }

    virtual const char* actionId() const override { return m_actionId.c_str(); }

    virtual int64_t timestampUs() const override { return m_timestampUs; }

protected:
    virtual void getObjectTrackId(Uuid* outValue) const override { *outValue = m_objectTrackId; }

    virtual void getDeviceId(Uuid* outValue) const override { *outValue = m_deviceId; }

    virtual IObjectTrackInfo* getObjectTrackInfo() const override
    {
        if (!m_objectTrackInfo)
            return nullptr;

        m_objectTrackInfo->addRef();
        return m_objectTrackInfo.get();
    }

    virtual const IStringMap* getParams() const override
    {
        m_params->addRef();
        return m_params.get();
    }

private:
    const std::string m_actionId;
    const Uuid m_objectTrackId;
    const Uuid m_deviceId;
    const int64_t m_timestampUs;

    const Ptr<IObjectTrackInfo> m_objectTrackInfo;
    const Ptr<const IStringMap> m_params;
};

} // namespace

QnExecuteAnalyticsActionRestHandler::QnExecuteAnalyticsActionRestHandler(
    QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QStringList QnExecuteAnalyticsActionRestHandler::cameraIdUrlParams() const
{
    return {"deviceId"};
}

int QnExecuteAnalyticsActionRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* /*owner*/)
{
    using namespace nx::network::http;

    const auto actionData = deserializeActionData(body);
    if (!actionData)
    {
        result.setError(QnJsonRestResult::InvalidParameter, "Invalid JSON object provided");
        return StatusCode::ok;
    }

    if (const auto errorMessage = checkInputParameters(*actionData); !errorMessage.isEmpty())
    {
        result.setError(QnJsonRestResult::InvalidParameter, errorMessage);
        return StatusCode::ok;
    }

    const auto engineResource = engineForAction(*actionData);
    if (!engineResource)
    {
        result.setError(QnJsonRestResult::InvalidParameter, "Engine not found");
        return StatusCode::ok;
    }

    const auto actionTypeDescriptor = actionDescriptor(*actionData);
    if (!actionTypeDescriptor)
    {
        result.setError(QnJsonRestResult::InvalidParameter, "Action type descriptor not found");
        return StatusCode::ok;
    }

    AnalyticsActionResult actionResult;
    auto extendedActionData = makeExtendedAnalyticsActionData(
        *actionData,
        engineResource,
        *actionTypeDescriptor);

    if (!extendedActionData)
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            "Unable to collect all needed Action data. Try to enable recording "
            "and/or wait some time until all the information needed for the Action "
            "is in the archive");
        return StatusCode::ok;
    }

    const QString errorMessage = executeAction(*extendedActionData, &actionResult);
    if (!errorMessage.isEmpty())
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lm("Engine %1 failed to execute Action: %2").args(engineResource, errorMessage));
    }

    result.setReply(actionResult);
    return StatusCode::ok;
}

std::optional<AnalyticsAction> QnExecuteAnalyticsActionRestHandler::deserializeActionData(
    const QByteArray& requestBody) const
{
    bool success = false;
    const auto actionData = QJson::deserialized<AnalyticsAction>(
        requestBody,
        AnalyticsAction(),
        &success);

    if (!success)
        return std::nullopt;

    return actionData;
}

QString QnExecuteAnalyticsActionRestHandler::checkInputParameters(
    const AnalyticsAction& actionData) const
{
    using namespace nx::vms::server;

    const auto makeErrorMessage =
        [](const QString& missingField)
        {
            return lm("Missing required field '%1'").args(missingField);
        };

    if (actionData.engineId.isNull())
        return makeErrorMessage("engineId");
    else if (actionData.actionId.isEmpty())
        return makeErrorMessage("actionId");
    else if (actionData.objectTrackId.isNull())
        return makeErrorMessage("trackId");

    return QString();
}

resource::AnalyticsEngineResourcePtr QnExecuteAnalyticsActionRestHandler::engineForAction(
    const AnalyticsAction& actionData) const
{
    const auto engineResource =
        resourcePool()->getResourceById<resource::AnalyticsEngineResource>(actionData.engineId);

    if (!engineResource)
        NX_WARNING(this, "Engine with id %1 not found", actionData.engineId);

    return engineResource;
}

std::optional<nx::vms::api::analytics::ActionTypeDescriptor>
    QnExecuteAnalyticsActionRestHandler::actionDescriptor(const AnalyticsAction& actionData) const
{
    nx::analytics::ActionTypeDescriptorManager actionTypeDescriptorManager(
        serverModule()->commonModule());

    const auto descriptor = actionTypeDescriptorManager.descriptor(actionData.actionId);
    if (!descriptor)
    {
        NX_WARNING(this,
            "Unable to find an action type descriptor for action type id %1", actionData.actionId);
    }

    return descriptor;
}

//-------------------------------------------------------------------------------------------------

std::optional<ExtendedAnalyticsActionData>
    QnExecuteAnalyticsActionRestHandler::makeExtendedAnalyticsActionData(
        const AnalyticsAction& action,
        const nx::vms::server::resource::AnalyticsEngineResourcePtr engineResource,
        const nx::vms::api::analytics::ActionTypeDescriptor& actionTypeDescriptor)
{
    if (!NX_ASSERT(engineResource,
        "Unable to create extended analytics action data without engine resource"))
    {
        return std::nullopt;
    }

    using namespace nx::vms::api::analytics;
    ExtendedAnalyticsActionData extendedAnalyticsActionData;
    extendedAnalyticsActionData.action = action;
    extendedAnalyticsActionData.engine = engineResource;
    extendedAnalyticsActionData.actionTypeDescriptor = actionTypeDescriptor;

    const auto& trackId = action.objectTrackId;
    const auto& capabilities = actionTypeDescriptor.requirements.capabilities;

    if (!capabilities)
    {
        NX_DEBUG(this, "The action type %1 has no addittional requirements."
            "Track, best shot frame and position are not going to be fetched.", action.actionId);
        return std::optional<ExtendedAnalyticsActionData>(extendedAnalyticsActionData);
    }

    const bool needBestShotVideoFrame = capabilities.testFlag(
        EngineManifest::ObjectAction::Capability::needBestShotVideoFrame);

    const bool needFullTrack =
        capabilities.testFlag(EngineManifest::ObjectAction::Capability::needFullTrack);

    const bool needBestShotObjectPosition = capabilities.testFlag(
        EngineManifest::ObjectAction::Capability::needBestShotObjectMetadata);

    const auto objectTrack = fetchObjectTrack(trackId, needFullTrack);
    if (!objectTrack)
    {
        NX_DEBUG(this, "Unable to fetch Track by id %1", trackId);
        return std::nullopt;
    }

    if (objectTrack->objectPosition.isEmpty())
    {
        NX_DEBUG(this, "Object Track %1 position sequence is empty", trackId);
        return std::nullopt;
    }

    if (needFullTrack)
        extendedAnalyticsActionData.objectTrack = objectTrack;

    if (!needBestShotObjectPosition && !needBestShotVideoFrame)
        return extendedAnalyticsActionData;

    // Either an Object position or the best shot video frame is needed.
    const int64_t bestShotTimestampUs = objectTrack->bestShot.initialized()
        ? objectTrack->bestShot.timestampUs
        : objectTrack->firstAppearanceTimeUs;

    if (!NX_ASSERT(bestShotTimestampUs != AV_NOPTS_VALUE))
    {
        NX_DEBUG(this,
            "Failed to fetch the best shot timestamp for the Object Track %1", trackId);
        return std::nullopt;
    }

    if (needBestShotObjectPosition)
    {
        if (objectTrack->bestShot.initialized())
        {
            extendedAnalyticsActionData.bestShotObjectPosition = objectTrack->bestShot;
        }
        else
        {
            extendedAnalyticsActionData.bestShotObjectPosition->timestampUs = objectTrack->firstAppearanceTimeUs;
            extendedAnalyticsActionData.bestShotObjectPosition->rect = objectTrack->objectPosition.boundingBox();
        }
    }

    if (needBestShotVideoFrame)
    {
        extendedAnalyticsActionData.bestShotVideoFrame = imageByTimestamp(
            action.deviceId,
            bestShotTimestampUs);
    }

    return extendedAnalyticsActionData;
}

/** @return Empty string on success, or a short error message on error.*/
QString QnExecuteAnalyticsActionRestHandler::executeAction(
    const ExtendedAnalyticsActionData& actionData,
    AnalyticsActionResult* outActionResult)
{
    static const QString kNoEngineToExecuteActionMessage(
        "No engine resource to execute action has been provided");

    if (!NX_ASSERT(actionData.engine, kNoEngineToExecuteActionMessage))
        return kNoEngineToExecuteActionMessage;

    const auto action = makePtr<Action>(actionData);
    static const QString kNoSdkEngineToExecuteActionMessage(
        "No SDK engine to execute the action has been provided");

    const wrappers::EnginePtr sdkEngine = actionData.engine->sdkEngine();
    if (!NX_ASSERT(sdkEngine, kNoSdkEngineToExecuteActionMessage))
        return kNoSdkEngineToExecuteActionMessage;

    const wrappers::Engine::ExecuteActionResult executeActionResult =
        sdkEngine->executeAction(action);

    if (executeActionResult.errorMessage.isEmpty()) //< Successfully executed the Action.
        *outActionResult = executeActionResult.actionResult;
    return executeActionResult.errorMessage;
}

std::optional<nx::analytics::db::ObjectTrack>
    QnExecuteAnalyticsActionRestHandler::fetchObjectTrack(
        const QnUuid& objectTrackId,
        bool needFullTrack)
{
    using namespace nx::analytics::db;
    Filter filter;
    filter.objectTrackId = objectTrackId;
    filter.maxObjectTrackSize = needFullTrack ? /*unlimited length*/ 0 : 1;

    NX_DEBUG(this,
        "Trying to fetch Track with id %1. Full Track is needed: %2",
        objectTrackId, needFullTrack);

    const auto lookupResult = makeDatabaseRequest(filter);
    if (!lookupResult)
    {
        NX_WARNING(this, "Unable to fetch an object appearance info for objectTrackId %1",
            objectTrackId);
        return std::nullopt;
    }

    NX_ASSERT(lookupResult->size() <= 1,
        lm("Only one object Track has been requested but got %1").args(lookupResult->size()));

    if (!lookupResult->empty())
        return lookupResult->at(0);

    NX_DEBUG(this, "Database lookup result is empty for object Track %1", objectTrackId);
    return std::nullopt;
}

CLVideoDecoderOutputPtr  QnExecuteAnalyticsActionRestHandler::imageByTimestamp(
    const QnUuid& deviceId,
    const int64_t timestampUs)
{
    NX_DEBUG(this, "Trying to fetch image by timestamp. Device id: %1, timestamp: %2",
        deviceId, timestampUs);

    const auto device = resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId);
    if (!device)
    {
        NX_WARNING(this, "Unable to find device %1", device);
        return CLVideoDecoderOutputPtr();
    }

    nx::api::ImageRequest imageRequest;
    imageRequest.usecSinceEpoch = timestampUs;
    imageRequest.imageFormat = nx::api::ImageRequest::ThumbnailFormat::raw;
    imageRequest.roundMethod = nx::api::ImageRequest::RoundMethod::precise;

    nx::api::CameraImageRequest cameraImageRequest(device, imageRequest);
    cameraImageRequest.streamSelectionMode =
        nx::api::CameraImageRequest::StreamSelectionMode::sameAsAnalytics;

    QnGetImageHelper helper(serverModule());
    CLVideoDecoderOutputPtr result = helper.getImage(cameraImageRequest);
    if (!result)
    {
        NX_DEBUG(this, "Unable to fetch image by timestamp %1. Device id: %2",
            deviceId, timestampUs);
    }

    return result;
}

std::optional<LookupResult> QnExecuteAnalyticsActionRestHandler::makeDatabaseRequest(
    const Filter& filter)
{
    NX_DEBUG(this, "Executing analytics database request with filter %1", filter);

    nx::utils::promise<std::tuple<ResultCode, LookupResult>> lookupCompleted;
    serverModule()->analyticsEventsStorage()->lookup(
        filter,
        [&lookupCompleted](ResultCode resultCode, LookupResult lookupResult)
        {
            lookupCompleted.set_value(
                std::make_tuple(resultCode, std::move(lookupResult)));
        });

    const auto result = lookupCompleted.get_future().get();
    const ResultCode resultCode = std::get<0>(result);
    if (resultCode != ResultCode::ok)
    {
        NX_WARNING(
            this,
            "Error occured while executing request to the analytics database with filter %1",
            filter);
        return std::nullopt;
    }

    return std::get<1>(result);
}

