#include "engine.h"

#include <map>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log_main.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include "device_agent.h"
#include "common.h"
#include "attributes_parser.h"
#include "string_helper.h"

namespace nx::vms_server_plugins::analytics::hanwha {

namespace {

static const QString kSamsungTechwinVendor("samsung");
static const QString kHanwhaTechwinVendor("hanwha");

static const QString kVideoAnalytics("VideoAnalytics");
static const QString kAudioAnalytics("AudioAnalytics");
static const QString kQueueEvent("QueueEvent");
static const std::map<QString, QString> kSpecialEventTypes = {
    {kVideoAnalytics, kVideoAnalytics},
    {kAudioAnalytics, kAudioAnalytics},
    {kQueueEvent, "Queue"}
};

// TODO: Decide on these unused constants.
static const std::chrono::milliseconds kAttributesTimeout(4000);
static const QString kAttributesPath("/stw-cgi/attributes.cgi/cgis");

QString specialEventName(const QString& eventName)
{
    const auto itr = kSpecialEventTypes.find(eventName);
    if (itr == kSpecialEventTypes.cend())
        return QString();

    return itr->second;
}

static bool isFirmwareActual(const std::string& firmwareVersion)
{
    // Firmware version has a format like this: 1.40.03_20190919_R32
    if (firmwareVersion.empty())
        return false;

    QString extendedVersion = QString::fromStdString(firmwareVersion);
    QStringList versionPartitions = extendedVersion.split('_'); //< {1.40.03 , 20190919 , R32}
    if (versionPartitions.empty())
        return false;

    QStringList versionList = versionPartitions[0].split('.'); //< {1 , 40 , 03}
    if (versionList.size() < 2)
        return false;

    int major = versionList[0].toInt();
    int minor = versionList[1].toInt();

    int revision = (versionList.size() >= 3) ? versionList[2].toInt() : 0;
    if (revision == 99 && (minor % 10) == 9)
        return false; //< beta version

    return major > 1 || (major == 1 && minor >= 41);
}

template <typename Pred>
QJsonValue filterJsonObjects(QJsonValue value, Pred pred)
{
    if (value.isObject())
    {
        auto object = value.toObject();
        if (!pred(object))
            return QJsonValue::Undefined;

        for (QJsonValueRef elementValueRef: object)
        {
            if (!(elementValueRef.isArray() || elementValueRef.isObject()))
                continue;

            elementValueRef = filterJsonObjects(elementValueRef, pred);
        }
        return object;
    }
    if (value.isArray())
    {
        auto array = value.toArray();
        for (auto it = array.begin(); it != array.end(); )
        {
            if (QJsonValue elementValue = filterJsonObjects(*it, pred); elementValue.isUndefined())
                it = array.erase(it);
            else
            {
                *it = elementValue;
                ++it;
            }
        }
        return array;
    }
    return value;
}

QJsonValue filterOutItemsAndSections(QJsonValue value)
{
    if (!value.isObject())
        return QJsonValue::Undefined;
    auto object = value.toObject();
    object["items"] = QJsonArray();
    object["sections"] = QJsonArray();
    return object;
}

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::SharedResources::SharedResources(
    const QString& sharedId,
    const Hanwha::EngineManifest& engineManifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
    :
    monitor(std::make_unique<MetadataMonitor>(engineManifest, url, auth)),
    sharedContext(std::make_shared<vms::server::plugins::HanwhaSharedResourceContext>(
        /*serverRuntimeEventManager*/ nullptr,
        /*resourcePool*/ nullptr,
        /*globalSettings*/ nullptr,
        sharedId))
{
    sharedContext->setResourceAccess(url, auth);
}

void Engine::SharedResources::setResourceAccess(
    const nx::utils::Url& url,
    const QAuthenticator& auth)
{
    sharedContext->setResourceAccess(url, auth);
    monitor->setResourceAccess(url, auth);
}

Engine::Engine(Plugin* plugin): m_plugin(plugin)
{
    QByteArray manifestData;
    QFile f(":/hanwha/manifest.json");
    if (f.open(QFile::ReadOnly))
        manifestData = f.readAll();
    {
        QFile file("plugins/hanwha/manifest.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this,
                lm("Switch to external manifest file %1").arg(QFileInfo(file).absoluteFilePath()));
            manifestData = file.readAll();
        }
    }
    m_manifest = QJson::deserialized<Hanwha::EngineManifest>(manifestData);
    m_manifest.InitializeObjectTypeMap();

    QByteArray attributeFiltersData;
    QFile attributeFiltersFile(":/hanwha/object_metadata_attribute_filters.json");
    if (attributeFiltersFile.open(QFile::ReadOnly))
        attributeFiltersData = attributeFiltersFile.readAll();
    {
        QFile file("plugins/hanwha/object_metadata_attribute_filters.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this,
                lm("Switch to external object metadata attribute filters file %1").arg(QFileInfo(file).absoluteFilePath()));
            attributeFiltersData = file.readAll();
        }
    }
    m_objectMetadataAttributeFilters = QJson::deserialized<Hanwha::ObjectMetadataAttributeFilters>(attributeFiltersData);
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    if (!isCompatible(deviceInfo))
    {
        *outResult = error(ErrorCode::otherError, "Device is not compatible with the Engine");
        return;
    }

    auto sharedRes = sharedResources(deviceInfo);
    auto sharedResourceGuard = nx::utils::makeScopeGuard(
        [&sharedRes, deviceInfo, this]()
        {
            if (sharedRes->deviceAgentCount == 0)
                m_sharedResources.remove(QString::fromUtf8(deviceInfo->sharedId()));
        });

    const bool isNvr = fetchIsNvr(sharedRes);

    // We need max resolution to translate relative coordinates to absolute and backward.
    // fetchMaxResolution may fail for some devices, in this case we get max resolution with
    // a special request later.
    const QSize maxResolution = fetchMaxResolution(sharedRes, deviceInfo);

    const auto deviceAgent = new DeviceAgent(this, deviceInfo, isNvr, maxResolution);

    const std::string firmwareVersion = deviceAgent->fetchFirmwareVersion();
    const bool areSettingsAndTrackingAllowed = isFirmwareActual(firmwareVersion);

    std::optional<QSet<QString>> eventTypeFilter;
    if (isNvr)
        eventTypeFilter = deviceAgent->getRealSupportedEventTypes();

    auto deviceAgentManifest = buildDeviceAgentManifest(sharedRes,
        deviceInfo, areSettingsAndTrackingAllowed, eventTypeFilter);

    if (!deviceAgentManifest)
        return;

    deviceAgent->setManifest(std::move(*deviceAgentManifest));

    deviceAgent->readCameraSettings();

    ++sharedRes->deviceAgentCount;

    *outResult = deviceAgent;
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new nx::sdk::String(QJson::serialized(m_manifest));
}

/*virtual*/ void Engine::setEngineInfo(const nx::sdk::analytics::IEngineInfo* /*engineInfo*/)
{
}

/*virtual*/ void Engine::doSetSettings(
    Result<const IStringMap*>* /*outResult*/, const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

/*virtual*/ void Engine::getPluginSideSettings(Result<const ISettingsResponse*>* /*outResult*/) const
{
}

/*virtual*/ void Engine::doExecuteAction(Result<IAction::Result>* /*outResult*/, const IAction* /*action*/)
{
}

std::optional<Hanwha::DeviceAgentManifest> Engine::buildDeviceAgentManifest(
    const std::shared_ptr<SharedResources>& sharedRes,
    const IDeviceInfo* deviceInfo,
    bool areSettingsAndTrackingAllowed,
    std::optional<QSet<QString>> filter) const
{
    Hanwha::DeviceAgentManifest deviceAgentManifest;

    const auto supportsObjectTracking = areSettingsAndTrackingAllowed
        && fetchSupportsObjectDetection(sharedRes, deviceInfo->channelNumber());

    if (supportsObjectTracking)
    {
        // DeviceAgent should understand all engine's object types.
        deviceAgentManifest.supportedObjectTypeIds.reserve(m_manifest.objectTypes.size());
        for (const nx::vms::api::analytics::ObjectType& objectType: m_manifest.objectTypes)
            deviceAgentManifest.supportedObjectTypeIds.push_back(objectType.id);
    }
    else
    {
        NX_DEBUG(this, "Device %1 (%2): doesn't support object detection/tracking.",
            deviceInfo->name(), deviceInfo->id());
    }

    std::optional<QSet<QString>> supportedEventTypeIds =
        fetchSupportedEventTypeIds(sharedRes, deviceInfo, filter);

    if (!supportedEventTypeIds)
    {
        NX_DEBUG(this, "Supported Event Type list is empty for the Device %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
        return std::nullopt;
    }
    if (supportsObjectTracking)
    {
        supportedEventTypeIds->insert("nx.hanwha.ObjectTracking.Start");
    }

    deviceAgentManifest.supportedEventTypeIds = QList<QString>::fromSet(*supportedEventTypeIds);

    if (!areSettingsAndTrackingAllowed)
    {
        deviceAgentManifest.deviceAgentSettingsModel = filterOutItemsAndSections(
            m_manifest.deviceAgentSettingsModel);
    }
    else
    {
        deviceAgentManifest.deviceAgentSettingsModel = filterJsonObjects(
            m_manifest.deviceAgentSettingsModel,
            [&](const QJsonObject& node) {
                bool keepByDefault = true;

                bool keepForObjectTracking = false;
                if (const auto required = node["requiredForObjectDetection"]; required.toBool())
                {
                    keepByDefault = false;
                    keepForObjectTracking = supportsObjectTracking;
                }

                bool keepForEvents = false;
                if (const auto eventTypeIds = node["requiredForEventTypeIds"].toArray();
                    !eventTypeIds.empty())
                {
                    keepByDefault = false;
                    for (const auto eventTypeId : eventTypeIds)
                    {
                        if (supportedEventTypeIds->contains(eventTypeId.toString()))
                        {
                            keepForEvents = true;
                            break;
                        }
                    }
                }

                return keepByDefault || keepForObjectTracking || keepForEvents;
            });
    }

    // Stream selection is always disabled.
    deviceAgentManifest.capabilities.setFlag(
        nx::vms::api::analytics::DeviceAgentManifest::disableStreamSelection);

    return deviceAgentManifest;
}

/*static*/ bool Engine::fetchSupportsObjectDetection(
    const std::shared_ptr<SharedResources>& sharedRes,
    int channel)
{
    const auto& information = sharedRes->sharedContext->information();
    if (!information)
        return false;

    const auto& attributes = information->attributes;
    if (!attributes.isValid())
        return false;

    // What Hanwha calls "object detection" we at NX usually call "object tracking".
    return attributes.attribute<bool>("Eventsource", "ObjectDetection", channel).value_or(false);
}

/*static*/ QSize Engine::fetchMaxResolution(const std::shared_ptr<SharedResources>& sharedRes,
    const nx::sdk::IDeviceInfo* deviceInfo)
{
    static const QString kNoMaxResolution = "0x0";

    if (const auto& information = sharedRes->sharedContext->information())
    {
        if (const auto& attributes = information->attributes; attributes.isValid())
        {
            const QString maxResolutionAsString = attributes.attribute<QString>(
                "Media", "MaxResolution", deviceInfo->channelNumber())
                .value_or(kNoMaxResolution);
            const auto maxResolutionAsList = maxResolutionAsString.split('x');
            if (maxResolutionAsList.size() == 2)
                return QSize(maxResolutionAsList[0].toInt(), maxResolutionAsList[1].toInt());
        }
    }

    return {};
}

/*static*/ bool Engine::fetchIsNvr(const std::shared_ptr<SharedResources>& sharedRes)
{
    if (const auto& information = sharedRes->sharedContext->information())
        return information->deviceType == nx::vms::server::plugins::HanwhaDeviceType::nvr;
    return false;
}

std::optional<QSet<QString>> Engine::fetchSupportedEventTypeIds(
    const std::shared_ptr<SharedResources>& sharedRes,
    const IDeviceInfo* deviceInfo,
    std::optional<QSet<QString>> filter) const
{
    const auto& information = sharedRes->sharedContext->information();
    if (!information)
    {
        NX_DEBUG(this, "Unable to fetch device information for %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
        return std::nullopt;
    }

    const auto& cgiParameters = information->cgiParameters;
    if (!cgiParameters.isValid())
    {
        NX_DEBUG(this, "CGI parameters are invalid for %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
        return std::nullopt;
    }

    const auto& eventStatuses = sharedRes->sharedContext->eventStatuses();
    if (!eventStatuses || !eventStatuses->isSuccessful())
    {
        NX_DEBUG(this, "Unable to fetch event statuses for %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
        return std::nullopt;
    }

    return eventTypeIdsFromParameters(
        sharedRes->sharedContext->url(),
        cgiParameters, eventStatuses.value, deviceInfo, filter);
}

std::optional<QSet<QString>> Engine::eventTypeIdsFromParameters(
    const nx::utils::Url& url,
    const nx::vms::server::plugins::HanwhaCgiParameters& parameters,
    const nx::vms::server::plugins::HanwhaResponse& eventStatuses,
    const IDeviceInfo* deviceInfo,
    std::optional<QSet<QString>> filter) const
{
    if (!parameters.isValid())
    {
        NX_DEBUG(this, "CGI parameters are invalid for %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
        return std::nullopt;
    }

    auto supportedEventTypesParameter = parameters.parameter(
        "eventstatus/eventstatus/monitor/Channel.#.EventType");

    if (!supportedEventTypesParameter.is_initialized())
    {
        NX_DEBUG(this, "Supported event types parameter is not initialzied for %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
        return std::nullopt;
    }

    QSet<QString> result;

    QStringList supportedEventTypes = supportedEventTypesParameter->possibleValues();
    const auto alarmInputParameter = parameters.parameter(
        "eventstatus/eventstatus/monitor/AlarmInput");

    if (alarmInputParameter)
    {
        NX_VERBOSE(this, "Adding %1 to supported event type list for %2 (%3)",
            alarmInputParameter->name(), deviceInfo->name(), deviceInfo->id());

        supportedEventTypes.push_back(alarmInputParameter->name());
    }

    if (filter)
    {
        QSet<QString> supportedEventTypesAsSet;
        for (const QString& string : supportedEventTypes)
            supportedEventTypesAsSet.insert(string);
        supportedEventTypesAsSet.intersect(*filter);
        supportedEventTypes.clear();
        for (const QString& string: supportedEventTypesAsSet)
            supportedEventTypes.push_back(string);
    }

    NX_VERBOSE(this,
        lm("camera %1 report supported analytics events %2").arg(url).arg(supportedEventTypes));
    for (const auto& eventTypeName: supportedEventTypes)
    {
        auto eventTypeId = m_manifest.eventTypeIdByName(eventTypeName);
        if (!eventTypeId.isEmpty())
        {
            NX_VERBOSE(this, "Adding %1 to supported event type list %2 (%3)",
                eventTypeId, deviceInfo->name(), deviceInfo->id());
            result.insert(eventTypeId);
        }

        const auto altEventName = specialEventName(eventTypeName);
        if (!altEventName.isEmpty())
        {
            const auto& responseParameters = eventStatuses.response();
            for (const auto& entry: responseParameters)
            {
                const auto& fullEventTypeName = entry.first;
                const bool isMatched =
                    fullEventTypeName.startsWith(lm("Channel.%1.%2.").args(
                        deviceInfo->channelNumber(), altEventName));

                if (isMatched)
                {
                    eventTypeId = m_manifest.eventTypeIdByName(fullEventTypeName);
                    if (!eventTypeId.isNull())
                    {
                        NX_VERBOSE(this, "Adding %1 to supported event type list %2 (%3)",
                            eventTypeId, deviceInfo->name(), deviceInfo->id());

                        result.insert(eventTypeId);
                    }
                }
            }
        }
    }

    NX_VERBOSE(this, "Supported event type list for %1 (%2): %3");
    return result;
}

const Hanwha::EngineManifest& Engine::manifest() const
{
    return m_manifest;
}

const Hanwha::ObjectMetadataAttributeFilters& Engine::objectMetadataAttributeFilters() const
{
    return m_objectMetadataAttributeFilters;
}

MetadataMonitor* Engine::monitor(
    const QString& sharedId,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
{
    std::shared_ptr<SharedResources> monitorCounter;
    {
        QnMutexLocker lock(&m_mutex);
        auto sharedResourcesItr = m_sharedResources.find(sharedId);
        if (sharedResourcesItr == m_sharedResources.cend())
        {
            sharedResourcesItr = m_sharedResources.insert(
                sharedId,
                std::make_shared<SharedResources>(
                    sharedId,
                    m_manifest,
                    url,
                    auth));
        }

        monitorCounter = sharedResourcesItr.value();
        ++monitorCounter->monitorUsageCount;
    }

    return monitorCounter->monitor.get();
}

void Engine::deviceAgentStoppedToUseMonitor(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto sharedResources = m_sharedResources.value(sharedId);
    if (!sharedResources)
        return;

    if (sharedResources->monitorUsageCount)
        --sharedResources->monitorUsageCount;

    if (sharedResources->monitorUsageCount <= 0)
        m_sharedResources[sharedId]->monitor->stopMonitoring();
}

void Engine::deviceAgentIsAboutToBeDestroyed(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto sharedResources = m_sharedResources.value(sharedId);
    if (!sharedResources)
        return;

    --sharedResources->deviceAgentCount;
    NX_ASSERT(sharedResources->deviceAgentCount >= 0);
    if (sharedResources->deviceAgentCount <= 0)
        m_sharedResources.remove(sharedId);
}

std::shared_ptr<Engine::SharedResources> Engine::sharedResources(const IDeviceInfo* deviceInfo)
{
    const nx::utils::Url url(deviceInfo->url());

    QAuthenticator auth;
    auth.setUser(deviceInfo->login());
    auth.setPassword(deviceInfo->password());

    QnMutexLocker lock(&m_mutex);
    auto sharedResourcesItr = m_sharedResources.find(deviceInfo->sharedId());
    if (sharedResourcesItr == m_sharedResources.cend())
    {
        sharedResourcesItr = m_sharedResources.insert(
            deviceInfo->sharedId(),
            std::make_shared<SharedResources>(
                deviceInfo->sharedId(),
                m_manifest,
                url,
                auth));
    }
    else
    {
        sharedResourcesItr.value()->setResourceAccess(url, auth);
    }

    return sharedResourcesItr.value();
}

/*virtual*/ void Engine::setHandler(IEngine::IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
}

/*virtual*/ bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const QString vendor = QString(deviceInfo->vendor()).toLower();
    return vendor.startsWith(kHanwhaTechwinVendor) || vendor.startsWith(kSamsungTechwinVendor);
}

std::shared_ptr<vms::server::plugins::HanwhaSharedResourceContext> Engine::sharedContext(
    QString m_sharedId)
{
    auto sharedResourcesItr = m_sharedResources.find(m_sharedId);
    if (sharedResourcesItr != m_sharedResources.cend())
    {
        return sharedResourcesItr.value()->sharedContext;
    }
    else
        return {};
}

} // namespace nx::vms_server_plugins::analytics::hanwha

namespace {

static const std::string kPluginManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.hanwha",
    "name": "Hanwha analytics",
    "description": "Access control solution designed to make it easier than ever to manage and monitoring facility access.",
    "version": "1.0.1",
    "vendor": "Hanwha"
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::hanwha::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
