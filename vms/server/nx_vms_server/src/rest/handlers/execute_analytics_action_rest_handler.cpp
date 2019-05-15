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
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/uuid.h>
#include <nx/vms_server_plugins/utils/uuid.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/helpers/object_track_info.h>
#include <nx/sdk/uuid.h>

#include <nx/analytics/action_type_descriptor_manager.h>

#include <camera/get_image_helper.h>
#include <analytics/db/abstract_storage.h>

#include <plugins/settings.h>

using namespace nx::vms::server;
using namespace nx::analytics::db;

namespace {

const QString kBestShotAttribute("nx.sys.preview.timestampUs");

nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackInfo> makeObjectTrackInfo(
    const ExtendedAnalyticsActionData& actionData)
{
    using namespace nx::vms::server::sdk_support;
    auto objectTrackInfo = nx::sdk::makePtr<nx::sdk::analytics::ObjectTrackInfo>();

    if (actionData.detectedObject)
    {
        const auto track = createObjectTrack(*actionData.detectedObject);
        if (!track)
            return nullptr;

        objectTrackInfo->setTrack(track.get());

        if (actionData.bestShotObjectPosition)
        {
            const auto bestShotObjectMetadata = createTimestampedObjectMetadata(
                *actionData.detectedObject,
                *actionData.bestShotObjectPosition);

            if (!bestShotObjectMetadata)
                return nullptr;

            objectTrackInfo->setBestShotObjectMetadata(bestShotObjectMetadata.get());
        }
    }

    if (actionData.bestShotVideoFrame)
    {
        nx::sdk::Ptr<nx::sdk::analytics::IUncompressedVideoFrame> bestShotVideoFrame =
            createUncompressedVideoFrame(
                actionData.bestShotVideoFrame,
                actionData.actionTypeDescriptor.requirements.bestShotVideoFramePixelFormat);

        if (!bestShotVideoFrame)
            return nullptr;

        objectTrackInfo->setBestShotVideoFrame(bestShotVideoFrame.get());
    }

    return objectTrackInfo;
}

class Action: public nx::sdk::RefCountable<nx::sdk::analytics::IAction>
{
public:
    Action(const ExtendedAnalyticsActionData& actionData, AnalyticsActionResult* actionResult):
        m_actionId(actionData.action.actionId.toStdString()),
        m_objectId(nx::vms_server_plugins::utils::fromQnUuidToSdkUuid(actionData.action.objectId)),
        m_deviceId(nx::vms_server_plugins::utils::fromQnUuidToSdkUuid(actionData.action.deviceId)),
        m_timestampUs(actionData.action.timestampUs),
        m_objectTrackInfo(makeObjectTrackInfo(actionData)),
        m_params(nx::vms::server::sdk_support::toIStringMap(actionData.action.params)),
        m_actionResult(actionResult)
    {
        NX_ASSERT(m_actionResult);
    }

    virtual const char* actionId() const override { return m_actionId.c_str(); }

    virtual nx::sdk::Uuid objectId() const override { return m_objectId; }

    virtual nx::sdk::Uuid deviceId() const override { return m_deviceId; }

    virtual nx::sdk::analytics::IObjectTrackInfo* objectTrackInfo() const override
    {
        if (!m_objectTrackInfo)
            return nullptr;

        m_objectTrackInfo->addRef();
        return m_objectTrackInfo.get();
    }

    virtual int64_t timestampUs() const
    {
        return m_timestampUs;
    }

    virtual const nx::sdk::IStringMap* params() const override
    {
        m_params->addRef();
        return m_params.get();
    }

    virtual void handleResult(const char* actionUrl, const char* messageToUser) override
    {
        m_actionResult->actionUrl = actionUrl;
        m_actionResult->messageToUser = messageToUser;
    }

private:
    std::string m_actionId;
    nx::sdk::Uuid m_objectId;
    nx::sdk::Uuid m_deviceId;
    int64_t m_timestampUs;

    nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackInfo> m_objectTrackInfo;
    const nx::sdk::Ptr<nx::sdk::IStringMap> m_params;

    AnalyticsActionResult* m_actionResult = nullptr;
};

/**
 * @return Null if no error.
 */
QString errorMessage(nx::sdk::Error error)
{
    switch (error)
    {
        case nx::sdk::Error::noError: return QString();
        case nx::sdk::Error::unknownError: return "error";
        case nx::sdk::Error::networkError: return "network error";
        default: return "unrecognized error";
    }
}

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
            "Unable to collect all needed action data");
        return StatusCode::ok;
    }

    const QString errorMessage = executeAction(*extendedActionData, &actionResult);
    if (!errorMessage.isEmpty())
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lm("Engine %1 failed to execute action: %2").args(engineResource, errorMessage));
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

    auto makeErrorMessage =
        [](const QString& missingField)
        {
            return lm("Missing required field '%1'").args(missingField);
        };

    if (actionData.engineId.isNull())
        return makeErrorMessage("engineId");
    else if (actionData.actionId.isEmpty())
        return makeErrorMessage("actionId");
    else if (actionData.objectId.isNull())
        return makeErrorMessage("objectId");

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

    const auto& trackId = action.objectId;
    const auto& capabilities = actionTypeDescriptor.requirements.capabilities;

    if (!capabilities)
    {
        NX_DEBUG(this, "The action type %1 has no addittional requirements."
            "Track, best shot frame and position are not going to be fetched.", action.actionId);
        return std::optional<ExtendedAnalyticsActionData>(extendedAnalyticsActionData);
    }

    const bool needBestShotTimestamp =
        capabilities.testFlag(
            EngineManifest::ObjectAction::Capability::needBestShotObjectMetadata)
        || capabilities.testFlag(
            EngineManifest::ObjectAction::Capability::needBestShotVideoFrame);

    const bool needToFetchFullTrack =
        capabilities.testFlag(EngineManifest::ObjectAction::Capability::needTrack);

    const bool needBestShotObjectPosition = capabilities.testFlag(
        EngineManifest::ObjectAction::Capability::needBestShotObjectMetadata);

    int64_t bestShotTimestampUs = AV_NOPTS_VALUE;
    if (needBestShotTimestamp)
        bestShotTimestampUs = tryToFindBestShotTimestampUsByAttrubute(trackId);

    if (bestShotTimestampUs != AV_NOPTS_VALUE)
    {
        if (needBestShotObjectPosition)
        {
            const auto position = fetchObjectPositionByTimestamp(trackId, bestShotTimestampUs);
            if (!position)
            {
                NX_WARNING(this,
                    "Unable to find best shot position for track %1 by timestamp %2 us",
                    trackId,
                    bestShotTimestampUs);
                return std::nullopt;
            }

            extendedAnalyticsActionData.bestShotObjectPosition = position;
        }

        if (needToFetchFullTrack || needBestShotObjectPosition)
        {
            extendedAnalyticsActionData.detectedObject = fetchDetectedObject(
                trackId,
                needToFetchFullTrack);
        }
    }
    else
    {
        auto detectedObject = fetchDetectedObject(trackId, needToFetchFullTrack);
        if (!detectedObject)
        {
            NX_DEBUG(this, "Unable fetch track by id %1", trackId);
            return std::nullopt;
        }

        const auto& bestShotObjectPosition = detectedObject->track[0];
        if (needBestShotTimestamp)
            bestShotTimestampUs = bestShotObjectPosition.timestampUsec;

        if (needToFetchFullTrack || needBestShotObjectPosition)
        {
            extendedAnalyticsActionData.bestShotObjectPosition = bestShotObjectPosition;
            extendedAnalyticsActionData.detectedObject = std::move(detectedObject);
        }
    }

    if (needBestShotTimestamp && bestShotTimestampUs != AV_NOPTS_VALUE)
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

    auto action = nx::sdk::makePtr<Action>(actionData, outActionResult);
    static const QString kNoSdkEngineToExecuteActionMessage(
        "No SDK engine to execute the action has been provided");

    const auto sdkEngine = actionData.engine->sdkEngine();
    if (!NX_ASSERT(sdkEngine, kNoSdkEngineToExecuteActionMessage))
        return kNoSdkEngineToExecuteActionMessage;

    nx::sdk::Error error = nx::sdk::Error::noError;
    sdkEngine->executeAction(action.get(), &error);

    // By this time, the Engine either already called Action::handleResult(), or is not going to
    // do it.
    return errorMessage(error);
}

std::optional<nx::analytics::db::ObjectPosition>
    QnExecuteAnalyticsActionRestHandler::fetchObjectPositionByTimestamp(
        const QnUuid& objectTrackId,
        int64_t timestampUs)
{
    using namespace nx::analytics::db;
    using namespace std::chrono;

    static const seconds kSearchTimeRangeDuration(2);

    NX_DEBUG(this,
        "Trying to fetch an object position by timestamp. Track id: %1, timestamp: %2",
        objectTrackId, timestampUs);

    Filter filter;
    filter.objectAppearanceId = objectTrackId;
    filter.timePeriod = QnTimePeriod(
        duration_cast<milliseconds>(microseconds(timestampUs) - kSearchTimeRangeDuration / 2),
        duration_cast<milliseconds>(kSearchTimeRangeDuration));
    filter.maxTrackSize = 0;

    const auto result = makeDatabaseRequest(filter);
    if (!result || result->empty())
        return std::nullopt;

    const auto& track = result->at(0).track;
    class Comparator
    {
    public:
        bool operator()(const ObjectPosition& position, int64_t timestamp) const
        {
            return position.timestampUsec > timestamp;
        };

        bool operator()(int64_t timestamp, const ObjectPosition& position) const
        {
            return position.timestampUsec > timestamp;
        }
    };

    const auto lowerBound =
        std::lower_bound(track.cbegin(), track.cend(), timestampUs, Comparator());

    const auto upperBound  =
        std::upper_bound(track.cbegin(), track.cend(), timestampUs, Comparator());

    if (lowerBound == track.cend() && upperBound == track.cend())
    {
        NX_DEBUG(this,
            "Unable to find position with timestamp %1 in the track %2",
            timestampUs, objectTrackId);

        return std::nullopt;
    }

    if (lowerBound == track.cend())
    {
        NX_DEBUG(this,
            "Unable to find lower bound for position with timestamp %1 in the track %2, "
            "using upper bound %3",
            timestampUs, objectTrackId, upperBound->timestampUsec);

        return *upperBound;
    }

    NX_DEBUG(this, "Got poistion for timestamp %1 for the track %2, position timestmap is %3",
        timestampUs, objectTrackId, lowerBound->timestampUsec);

    return *lowerBound;
}

std::optional<nx::analytics::db::DetectedObject>
    QnExecuteAnalyticsActionRestHandler::fetchDetectedObject(
        const QnUuid& objectTrackId,
        bool needFullTrack)
{
    using namespace nx::analytics::db;
    Filter filter;
    filter.objectAppearanceId = objectTrackId;
    filter.maxTrackSize = needFullTrack ? /*unlimited length*/ 0 : 1;

    NX_DEBUG(this,
        "Trying to fetch track with id %1. Full track is needed: %2",
        objectTrackId, needFullTrack);

    const auto lookupResult = makeDatabaseRequest(filter);
    if (!lookupResult)
    {
        NX_WARNING(this, "Unable to fetch an object appearance info for objectTrackId %1",
            objectTrackId);
        return std::nullopt;
    }

    NX_ASSERT(lookupResult->size() <= 1,
        lm("Only one object track has been requested but got %1").args(lookupResult->size()));

    if (!lookupResult->empty())
        return lookupResult->at(0);

    NX_DEBUG(this, "Database lookup result is empty for object track %1", objectTrackId);
    return std::nullopt;
}

int64_t QnExecuteAnalyticsActionRestHandler::tryToFindBestShotTimestampUsByAttrubute(
    const QnUuid& objectTrackId)
{
    using namespace nx::analytics::db;
    Filter filter;
    filter.objectAppearanceId = objectTrackId;
    filter.maxTrackSize = 1;

    // It's a hack needed because filter.requiredAttrributes field doesn't work.
    filter.freeText = kBestShotAttribute;

    NX_DEBUG(this,
        "Trying to fetch best shot timestamp by attribute for the track %1",
        objectTrackId);

    auto lookupResult = makeDatabaseRequest(filter);
    if (!lookupResult || lookupResult->empty())
    {
        NX_DEBUG(this, "Unable to find a best shot for object track %1", objectTrackId);
        return AV_NOPTS_VALUE;
    }

    for (const auto& attribute: lookupResult->at(0).attributes)
    {
        if (attribute.name != kBestShotAttribute)
            continue;

        bool success = false;
        int64_t timestampUs = attribute.value.toLongLong(&success);
        if (success)
        {
            NX_DEBUG(this,
                "Best shot timestamp has been found by attribute for the track %1: %2",
                objectTrackId, timestampUs);

            return timestampUs;
        }
        else
        {
            NX_WARNING(this,
                "Unable to convert best shot timestamp attribute value to a number. "
                "Track id: %1, attribute value: %2",
                objectTrackId, attribute.value);
        }

        break;
    }

    return AV_NOPTS_VALUE;
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

std::unique_ptr<sdk_support::AbstractManifestLogger>
QnExecuteAnalyticsActionRestHandler::makeLogger(
    resource::AnalyticsEngineResourcePtr engineResource) const
{
    const QString messageTemplate(
        "Error occurred while fetching Engine manifest for engine: {:engine}: {:error}");

    return std::make_unique<sdk_support::ManifestLogger>(
        typeid(this), //< Using the same tag for all instances.
        messageTemplate,
        std::move(engineResource));
}

