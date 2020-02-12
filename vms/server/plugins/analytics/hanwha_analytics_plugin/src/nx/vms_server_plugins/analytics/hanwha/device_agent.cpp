#include "device_agent.h"

#include <chrono>
#include <sstream>
#include <iostream>
#include <charconv>

#include <QtCore/QUrl>

#define NX_PRINT_PREFIX "[hanwha::DeviceAgent] "
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/timestamped_object_metadata.h>
#include <nx/sdk/analytics/i_custom_metadata_packet.h>

#include <nx/utils/log/log.h>
#include <nx/vms/server/analytics/predefined_attributes.h>

#include <nx/sdk/helpers/string_map.h>

#include "common.h"
#include "objectMetadataXmlParser.h"
#include "settings_model.h"

namespace nx::vms_server_plugins::analytics::hanwha {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

/**
 * Extract frameSize from json string. If fail, returns std::nullopt.
 * \return error message, if jsonReply indicates an error and contains an error message
 *     empty string, if jsonReply indicates an error and contains no error message
 *     std::nullopt, if jsonReply does not indicates an error
 */
std::optional<std::string> parseError(const std::string& jsonReply)
{
    std::string error;
    nx::kit::Json json = nx::kit::Json::parse(jsonReply, error);
    if (!json.is_object())
        return std::nullopt; //< not an error

    if (json["Response"].string_value() == "Fail")
        return json["Error"]["Details"].string_value(); //< may be empty

    return std::nullopt; //< not an error
}

/** Extract frameSize from json string. If fail, returns std::nullopt. */
static std::optional<FrameSize> parseFrameSize(const std::string& jsonReply)
{
    std::string error;
    nx::kit::Json json = nx::kit::Json::parse(jsonReply, error);
    if (!json.is_object())
        return std::nullopt;

    const std::vector<nx::kit::Json>& videoProfiles = json["VideoProfiles"].array_items();
    if (videoProfiles.empty())
        return std::nullopt;

    const std::vector<nx::kit::Json>& profiles = videoProfiles[0]["Profiles"].array_items();
    if (videoProfiles.empty())
        return std::nullopt;

    FrameSize maxFrameSize;
    for (const nx::kit::Json& profile : profiles)
    {
        std::string resolution = profile["Resolution"].string_value();
        std::istringstream stream(resolution);
        FrameSize profileFrameSize;
        char x;
        stream >> profileFrameSize.width >> x >> profileFrameSize.height;
        bool bad = !stream;

        if (!bad && x == 'x' && maxFrameSize < profileFrameSize)
            maxFrameSize = profileFrameSize;

    }
    return maxFrameSize;
}

/** Extract frameSize from json string. If fail, returns std::nullopt. */
static std::optional<SupportedEventCategories> parseSupportedEventCategories(
    const std::string& jsonReply)
{
    SupportedEventCategories result = {false};

    std::string error;
    nx::kit::Json json = nx::kit::Json::parse(jsonReply, error);
    if (!json.is_object())
        return std::nullopt;

    json = json["ChannelEvent"];
    if (!json.is_array() || json.array_items().empty())
        return std::nullopt;
    json = json.array_items().front();

    result[int(EventCategory::motionDetection)] = json["MotionDetection"].is_bool();
    result[int(EventCategory::faceDetectionGeneral)] = json["FaceDetectionGeneral"].is_bool();
    result[int(EventCategory::tampering)] = json["Tampering"].is_bool();
    result[int(EventCategory::audioDetection)] = json["AudioDetection"].is_bool();
    result[int(EventCategory::defocusDetection)] = json["DefocusDetection"].is_bool();
    result[int(EventCategory::fogDetection)] = json["FogDetection"].is_bool();
    result[int(EventCategory::faceDetectionGeneral)] = json["FaceDetection"].is_bool();
    result[int(EventCategory::videoAnalytics)] = json["VideoAnalytics"].is_bool();
    result[int(EventCategory::audioAnalytics)] = json["AudioAnalytics"].is_bool();
    result[int(EventCategory::queues)] = json["QueueEvent"].is_bool();
    result[int(EventCategory::shockDetection)] = json["ShockDetection"].is_bool();
    result[int(EventCategory::objectDetection)] = json["ObjectDetection"].is_bool();

    return result;
}

/**
 * The core function - transfers settings from server to device.
 * Read a bunch of settings `settingGroup` from the server (from `sourceMap`) and write it
 * to the agent's device, using `sendingFunction`.
 */
template<class SettingGroupT, class SendingFunctor>
void copySettingsFromServerToCamera(nx::sdk::Ptr<nx::sdk::StringMap>& errorMap,
    const nx::sdk::IStringMap* sourceMap,
    SettingGroupT& previousState,
    SendingFunctor sendingFunctor,
    FrameSize frameSize,
    int channelNumber,
    int objectIndex = -1)
{
    SettingGroupT settingGroup;
    if (!readSettingsFromServer(sourceMap, &settingGroup, objectIndex))
    {
        settingGroup.replanishErrorMap(errorMap, "read failed");
        return;
    }

    if (!differesEnough(settingGroup, previousState))
        return;

    const std::string settingQuery =
        settingGroup.buildCameraWritingQuery(frameSize, channelNumber);

    const std::string error = sendingFunctor(settingQuery);
    if (!error.empty())
        settingGroup.replanishErrorMap(errorMap, error);
    else
        previousState = settingGroup;
}

} // namespace

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    m_engine(engine)
{
    this->setDeviceInfo(deviceInfo);
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    m_engine->deviceAgentIsAboutToBeDestroyed(m_sharedId);
}

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    const auto eventTypeIds = neededMetadataTypes->eventTypeIds();
    if (const char* const kMessage = "Event type id list is null";
        !NX_ASSERT(eventTypeIds, kMessage))
    {
        *outResult = error(ErrorCode::internalError, kMessage);
        return;
    }

    stopFetchingMetadata();

    if (eventTypeIds->count() != 0)
        *outResult = startFetchingMetadata(neededMetadataTypes);
}

namespace
{
    template<class T>
    Uuid deviceObjectNumberToUuid(T deviceObjectNumber)
    {
        Uuid serverObjectNumber;
        memcpy(&serverObjectNumber, &deviceObjectNumber,
            std::min(sizeof(serverObjectNumber), sizeof(deviceObjectNumber)));
        return serverObjectNumber;
    }
}

/**
 * Receives `dataPacket` from Server. This packet is a `CustomMetadataPacket`, that contains xml
 * with the information about events occurred. The function parses the xml and extracts
 * the information about objects. Then it creates `objectMetadataPacket, attaches `objectMetadata`
 * (with object information) to it and sends to server using `m_handler`.
*/
void DeviceAgent::doPushDataPacket(Result<void>* outResult, IDataPacket* dataPacket)
{
    const auto incomingPacket = dataPacket->queryInterface<ICustomMetadataPacket>();
    QByteArray xmlData(incomingPacket->data(), incomingPacket->dataSize());

    static int i = 0;
    std::cout << "========================" << std::endl;
    std::cout << ++i << std::endl << (const char*)xmlData << std::endl << std::endl << std::endl;

    const auto ts = incomingPacket->timestampUs();

    // `outcomingPackets` vector contains packets each of which contains exactly one object
    // we'll pass each packet to `m_handler->handleMetadata` function.
    // For now client does not support packets with multiple objects. When GUI team fix it
    // we'll pass one packet with many objects instead of many packets with one object.

    Ptr<ObjectMetadataPacket> outcomingPacket = parseObjectMetadataXml(
        xmlData, m_engine->engineManifest());

    if (outcomingPacket && outcomingPacket->count())
    {
        std::cout << outcomingPacket->count() << std::endl;

        outcomingPacket->setTimestampUs(ts);
        outcomingPacket->setDurationUs(1'000'000); // 1 second

        if (NX_ASSERT(m_handler))
            m_handler->handleMetadata(outcomingPacket.get());
    }

/*
    // This chunk of code is for test purposes.
    Ptr<ObjectMetadataPacket> outcomingPacket = makePtr<ObjectMetadataPacket>();
    outcomingPacket->setTimestampUs(ts);
    outcomingPacket->setDurationUs(1'000'000);

    auto objectMetadata = makePtr<ObjectMetadata>();
    int objectId = 100;
    static const nx::sdk::Uuid trackId =
        nx::vms_server_plugins::analytics::hanwha::deviceObjectNumberToUuid(objectId);
    objectMetadata->setTrackId(trackId);
    objectMetadata->setTypeId("nx.hanwha.ObjectDetection.Head");
    objectMetadata->setBoundingBox(Rect(0.25, 0.25, 0.5, 0.5));
    outcomingPacket->addItem(objectMetadata.get());

    if (NX_ASSERT(m_handler))
        m_handler->handleMetadata(outcomingPacket.get());
//*/
}

/**
 * Reads settings from `sourceMap` and writes them into the agent's device. If a setting can not
 * be written, its name is added to `outResult` error map.h
*/
void DeviceAgent::doSetSettings(
    Result<const IStringMap*>* outResult, const IStringMap* sourceMap)
{
    if (!m_serverHasSentInitialSettings)
    {
        // we should ignore initial settings
        m_serverHasSentInitialSettings = true;
        return;
    }

    int c = sourceMap->count();
    //std::shared_ptr<vms::server::plugins::HanwhaSharedResourceContext> m_engine->sharedContext(m_sharedId);

    auto errorMap = makePtr<nx::sdk::StringMap>();

    const auto sender = [this](const std::string& s) {return this->sendWritingRequestToDeviceSync(s); };

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.shockDetection, sender, m_frameSize, m_channelNumber);

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.motion, sender, m_frameSize, m_channelNumber);

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.motionDetectionObjectSize, sender, m_frameSize, m_channelNumber);

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.ivaObjectSize, sender, m_frameSize, m_channelNumber);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
    {
        copySettingsFromServerToCamera(errorMap, sourceMap,
            m_settings.motionDetectionIncludeArea[i], sender, m_frameSize, m_channelNumber, i);
    }

    for (int i = 0; i < Settings::kMultiplicity; ++i)
    {
        copySettingsFromServerToCamera(errorMap, sourceMap,
            m_settings.motionDetectionExcludeArea[i], sender, m_frameSize, m_channelNumber, i);
    }

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.tamperingDetection, sender, m_frameSize, m_channelNumber);

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.defocusDetection, sender, m_frameSize, m_channelNumber);

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.fogDetection, sender, m_frameSize, m_channelNumber);

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.objectDetectionGeneral, sender, m_frameSize, m_channelNumber);

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.objectDetectionBestShot, sender, m_frameSize, m_channelNumber);

    for (int i = 0; i < 8; ++i)
    {
        copySettingsFromServerToCamera(errorMap, sourceMap,
            m_settings.objectDetectionExcludeArea[i], sender, m_frameSize, m_channelNumber, i);
    }

    for (int i = 0; i < 8; ++i)
    {
        copySettingsFromServerToCamera(errorMap, sourceMap,
            m_settings.ivaLine[i], sender, m_frameSize, m_channelNumber, i);
    }

    for (int i = 0; i < 8; ++i)
    {
        copySettingsFromServerToCamera(errorMap, sourceMap,
            m_settings.ivaIncludeArea[i], sender, m_frameSize, m_channelNumber, i);
    }

    for (int i = 0; i < 8; ++i)
    {
        copySettingsFromServerToCamera(errorMap, sourceMap,
            m_settings.ivaExcludeArea[i], sender, m_frameSize, m_channelNumber, i);
    }

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.audioDetection, sender, m_frameSize, m_channelNumber);

    copySettingsFromServerToCamera(errorMap, sourceMap,
        m_settings.soundClassification, sender, m_frameSize, m_channelNumber);

    *outResult = errorMap.releasePtr();
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    const auto response = new nx::sdk::SettingsResponse();
    m_settings.shockDetection.writeToServer(response);

    m_settings.motion.writeToServer(response);

    m_settings.motionDetectionObjectSize.writeToServer(response);

    m_settings.ivaObjectSize.writeToServer(response);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        m_settings.motionDetectionIncludeArea[i].writeToServer(response);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        m_settings.motionDetectionExcludeArea[i].writeToServer(response);

    m_settings.tamperingDetection.writeToServer(response);

    m_settings.defocusDetection.writeToServer(response);

    m_settings.fogDetection.writeToServer(response);

    m_settings.objectDetectionGeneral.writeToServer(response);

    m_settings.objectDetectionBestShot.writeToServer(response);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        m_settings.objectDetectionExcludeArea[i].writeToServer(response);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        m_settings.ivaLine[i].writeToServer(response);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        m_settings.ivaIncludeArea[i].writeToServer(response);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        m_settings.ivaExcludeArea[i].writeToServer(response);

    m_settings.audioDetection.writeToServer(response);

    m_settings.soundClassification.writeToServer(response);

    *outResult = response;
}

Result<void> DeviceAgent::startFetchingMetadata(const IMetadataTypes* /*metadataTypes*/)
{
    const auto monitorHandler =
        [this](const EventList& events)
        {
            using namespace std::chrono;
            auto eventMetadataPacket = makePtr<EventMetadataPacket>();

            for (const auto& hanwhaEvent: events)
            {
                if (hanwhaEvent.channel.is_initialized() && hanwhaEvent.channel != m_channelNumber)
                    return;

                auto eventMetadata = makePtr<EventMetadata>();
                NX_PRINT
                    << "Got event: caption ["
                    << hanwhaEvent.caption.toStdString() << "], description ["
                    << hanwhaEvent.description.toStdString() << "], "
                    << "channel " << m_channelNumber;

                NX_VERBOSE(this, lm("Got event: %1 %2 on channel %3").args(
                    hanwhaEvent.caption, hanwhaEvent.description, m_channelNumber));

                eventMetadata->setTypeId(hanwhaEvent.typeId.toStdString());
                eventMetadata->setCaption(hanwhaEvent.caption.toStdString());
                eventMetadata->setDescription(hanwhaEvent.caption.toStdString());
                eventMetadata->setIsActive(hanwhaEvent.isActive);
                eventMetadata->setConfidence(1.0);
                eventMetadata->addAttribute(makePtr<Attribute>(
                    IAttribute::Type::string,
                    nx::vms::server::analytics::kInputPortIdAttribute,
                    hanwhaEvent.fullEventName.toStdString()));

                eventMetadataPacket->setTimestampUs(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                eventMetadataPacket->setDurationUs(-1);
                eventMetadataPacket->addItem(eventMetadata.get());
            }

            if (NX_ASSERT(m_handler))
                m_handler->handleMetadata(eventMetadataPacket.get());
        };

    if (!NX_ASSERT(m_engine))
        return error(ErrorCode::internalError, "Unable to access the Engine");

    m_monitor = m_engine->monitor(m_sharedId, m_url, m_auth);
    if (!m_monitor)
        return error(ErrorCode::internalError, "Unable to access the monitor");

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return {};
}

void DeviceAgent::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_engine);
    if (m_engine)
        m_engine->deviceAgentStoppedToUseMonitor(m_sharedId);

    m_monitor = nullptr;
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    if (m_manifest.isEmpty())
        *outResult = error(ErrorCode::internalError, "DeviceAgent manifest is empty");
    else
        *outResult = new nx::sdk::String(m_manifest);
}

void DeviceAgent::setDeviceInfo(const IDeviceInfo* deviceInfo)
{
    m_url = deviceInfo->url();
    m_model = deviceInfo->model();
    m_firmware = deviceInfo->firmware();
    m_auth.setUser(deviceInfo->login());
    m_auth.setPassword(deviceInfo->password());
    m_uniqueId = deviceInfo->id();
    m_sharedId = deviceInfo->sharedId();
    m_channelNumber = deviceInfo->channelNumber();
}

void DeviceAgent::setDeviceAgentManifest(const QByteArray& manifest)
{
    m_manifest = manifest;
}

void DeviceAgent::setMonitor(MetadataMonitor* monitor)
{
    m_monitor = monitor;
}

std::unique_ptr<nx::network::http::HttpClient> DeviceAgent::createSettingsHttpClient() const
{
    auto result = std::make_unique<nx::network::http::HttpClient>();
    result->setUserName(m_auth.user());
    result->setUserPassword(m_auth.password());
    result->addAdditionalHeader("Accept", "application/json");
    return result;
}

/**
 * Synchronously send the request that should change analytics settings on the agent's device.
 * \return empty string, if the request is successful
 *     string with error message, if not
 */
std::string DeviceAgent::sendWritingRequestToDeviceSync(const std::string& query)
{
    nx::utils::Url command(m_url);
    constexpr const char* kEventPath = "/stw-cgi/eventsources.cgi";
    command.setPath(kEventPath);
    command.setQuery(QString::fromStdString(query));

    auto settingsHttpClient = createSettingsHttpClient();

    const bool isSent = settingsHttpClient->doGet(command);
    if (!isSent)
        return "Failed to send command to camera";

    auto* response = settingsHttpClient->response();
    if (response->statusLine.statusCode != 200)
        return response->statusLine.toString().toStdString();

    /* Camera may return 200 OK, and contain error description in the body:
    {
        "Response": "Fail",
        "Error" :
         {
            "Code": 600,
            "Details" : "Submenu Not Found"
        }
    }
    */
    auto messageBodyOptional = settingsHttpClient->fetchEntireMessageBody();
    if (messageBodyOptional.has_value())
    {
        std::string body = messageBodyOptional->toStdString();
        auto errorMessage = parseError(body);
        if (!errorMessage)
            return {};

        if (errorMessage->empty())
            return "Device answered with undescribed error";
        else
            return *errorMessage;
    }
    return {};
}

/**
 * Synchronously send reading request to the agent's device.
 * Typical parameter tuples for `domain/submenu/action`:
 *     eventstatus/eventstatus/check
 *     eventsources/tamperingdetection/view
 *     media/videoprofile/view
 * /return the answer from the device or empty string, if no answer received.
*/
std::string DeviceAgent::sendReadingRequestToDeviceSync(
    const char* domain, const char* submenu, const char* action)
{
    std::string result;
    nx::utils::Url command(m_url);
    constexpr const char* kPathPattern = "/stw-cgi/%1.cgi";
    constexpr const char* kQueryPattern = "msubmenu=%1&action=%2&Channel=%3";
    command.setPath(QString::fromStdString(kPathPattern).arg(domain));
    command.setQuery(QString::fromStdString(kQueryPattern).arg(submenu).arg(action).arg(m_channelNumber));

    auto settingsHttpClient = createSettingsHttpClient();

    const bool isSent = settingsHttpClient->doGet(command);
    if (!isSent)
        return result;

    auto* response = settingsHttpClient->response();
    const bool isApproved = (response->statusLine.statusCode == 200);
    const std::string sl = response->statusLine.toString().toStdString();

    auto messageBodyOptional = settingsHttpClient->fetchEntireMessageBody();
    if (messageBodyOptional.has_value())
        result = messageBodyOptional->toStdString();
    else
        ;//NX_DEBUG(logTag, "makeActiRequest: Error getting response body.");
    return result; // nx::network::http::StatusCode::internalServerError;
}

std::string DeviceAgent::loadEventSettings(const char* eventName)
{
    return sendReadingRequestToDeviceSync("eventsources", eventName, "view");
}

/** Load max resolution from the device. */
void DeviceAgent::loadFrameSize()
{
    /* http://<ip>/stw-cgi/media.cgi?msubmenu=videoprofile&action=view&Channel=0 */
    std::string jsonReply = sendReadingRequestToDeviceSync("media", "videoprofile", "view");

    std::optional<FrameSize> frameSize = parseFrameSize(jsonReply);
    if (frameSize)
        m_frameSize = *frameSize;
}

void DeviceAgent::loadSupportedEventTypes()
{
    /* http://<ip>/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=check&Channel=0 */
    std::string jsonReply = sendReadingRequestToDeviceSync("eventstatus", "eventstatus", "check");
    std::optional<SupportedEventCategories> cats = parseSupportedEventCategories(jsonReply);

}

void DeviceAgent::readCameraSettings()
{
    std::string sunapiReply;

    loadFrameSize();

    ////loadSupportedEventTypes();

    sunapiReply = loadEventSettings("shockdetection");
    readFromDeviceReply(sunapiReply, &m_settings.shockDetection, m_frameSize, m_channelNumber);

    sunapiReply = loadEventSettings("tamperingdetection");
    readFromDeviceReply(sunapiReply, &m_settings.tamperingDetection, m_frameSize, m_channelNumber);

    sunapiReply = loadEventSettings("videoanalysis2");
    readFromDeviceReply(sunapiReply, &m_settings.motion, m_frameSize, m_channelNumber);

    readFromDeviceReply(sunapiReply, &m_settings.motionDetectionObjectSize, m_frameSize, m_channelNumber);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        readFromDeviceReply(sunapiReply, &m_settings.motionDetectionIncludeArea[i], m_frameSize, m_channelNumber, i);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        readFromDeviceReply(sunapiReply, &m_settings.motionDetectionExcludeArea[i], m_frameSize, m_channelNumber, i);

    readFromDeviceReply(sunapiReply, &m_settings.ivaObjectSize, m_frameSize, m_channelNumber);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        readFromDeviceReply(sunapiReply, &m_settings.ivaLine[i], m_frameSize, m_channelNumber, i);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        readFromDeviceReply(sunapiReply, &m_settings.ivaIncludeArea[i], m_frameSize, m_channelNumber, i);

    for (int i = 0; i < Settings::kMultiplicity; ++i)
        readFromDeviceReply(sunapiReply, &m_settings.ivaExcludeArea[i], m_frameSize, m_channelNumber, i);

    sunapiReply = loadEventSettings("defocusdetection");
    readFromDeviceReply(sunapiReply, &m_settings.defocusDetection, m_frameSize, m_channelNumber);

    sunapiReply = loadEventSettings("fogdetection");
    readFromDeviceReply(sunapiReply, &m_settings.fogDetection, m_frameSize, m_channelNumber);

    //sunapiReply = loadEventSettings("facedetection");
    //readFromDeviceReply(sunapiReply, &m_settings.faceDetectionGeneral, m_frameSize, m_channelNumber);

    sunapiReply = loadEventSettings("objectdetection");
    readFromDeviceReply(sunapiReply, &m_settings.objectDetectionGeneral, m_frameSize, m_channelNumber);
    for (int i = 0; i < Settings::kMultiplicity; ++i)
        readFromDeviceReply(sunapiReply, &m_settings.objectDetectionExcludeArea[i], m_frameSize, m_channelNumber, i);

    sunapiReply = loadEventSettings("metaimagetransfer");
    readFromDeviceReply(sunapiReply, &m_settings.objectDetectionBestShot, m_frameSize, m_channelNumber);

    sunapiReply = loadEventSettings("audiodetection");
    readFromDeviceReply(sunapiReply, &m_settings.audioDetection, m_frameSize, m_channelNumber);

    sunapiReply = loadEventSettings("audioanalysis");
    readFromDeviceReply(sunapiReply, &m_settings.soundClassification, m_frameSize, m_channelNumber);
}

void DeviceAgent::addSettingModel(nx::vms::api::analytics::DeviceAgentManifest* destinastionManifest)
{
    QString model = deviceAgentSettingsModel();

    QJsonDocument doc = QJsonDocument::fromJson(model.toUtf8());
    if (doc.isObject())
        destinastionManifest->deviceAgentSettingsModel = doc.object();
}

} // namespace nx::vms_server_plugins::analytics::hanwha
