#include "device_agent_builder.h"

#include <string>
#include <map>

#define NX_PRINT_PREFIX "[hanwha::DeviceAgent] "
#include <nx/kit/debug.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include <nx/utils/std/algorithm.h>

#include "device_agent.h"
#include "settings_processor.h"
#include "common.h"
#include "device_response_json_parser.h"

namespace nx::vms_server_plugins::analytics::hanwha {

using namespace nx::vms::server; //< all plugins::xxx classes are taken from this namespace
//-------------------------------------------------------------------------------------------------

namespace{

static const QString kFetchErrorPattern =
    "Unable to fetch %1 from Hanwha shared context for %2. %3";

const static QString kVideoAnalyticsFamilyName = "VideoAnalytics";
const static QString kAudioAnalyticsFamilyName = "AudioAnalytics";
const static QString kQueueAnalyticsFamilyName = "Queue";

bool isPopulousFamily(const QString& eventTypeFamilyName)
{
    for (const QString& validPrefix :
        { kVideoAnalyticsFamilyName, kAudioAnalyticsFamilyName, kQueueAnalyticsFamilyName })
    {
        if (eventTypeFamilyName.startsWith(validPrefix))
            return true;
    }
    return false;
}

const QStringList fixFamilyNames(const QStringList& eventTypeFamilyNames)
{
    QStringList result(eventTypeFamilyNames);
    result.reserve(eventTypeFamilyNames.size());

    for (QString& familyName: result)
    {
        if (familyName == "QueueEvent")
            familyName = kQueueAnalyticsFamilyName;
    }
    return result;
}

QStringList listIntersection(const QStringList& list1, const QStringList& list2)
{
    auto set1 = QSet<QString>::fromList(list1);
    const auto set2 = QSet<QString>::fromList(list2);
    QStringList result = QStringList::fromSet(set1.intersect(set2));

    result.sort(); //< for convenient debug
    return result;
}

template <typename Pred>
QJsonValue filterJsonObjects(QJsonValue value, Pred pred)
{
    if (value.isObject())
    {
        auto object = value.toObject();
        if (!pred(object))
            return QJsonValue::Undefined;

        for (QJsonValueRef elementValueRef : object)
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

} // namespace

//-------------------------------------------------------------------------------------------------

DeviceAgentBuilder::Information::Information(
    InformationTypeDirect,
    std::shared_ptr<nx::vms::server::plugins::HanwhaSharedResourceContext>& context,
    int channelNumber)
{
    plugins::HanwhaResult<plugins::HanwhaInformation> informationResult = context->information();
    if (informationResult)
    {
        this->attributes = std::move(informationResult->attributes);
        this->cgiParameters = std::move(informationResult->cgiParameters);
    }

    const plugins::HanwhaResult<plugins::HanwhaResponse> eventStatuses = context->eventStatuses();
    if (eventStatuses && eventStatuses->isSuccessful())
        this->eventStatusMap = eventStatuses->response();
    this->channelNumber = channelNumber;
    this->isNvr = (informationResult->deviceType == plugins::HanwhaDeviceType::nvr);
}

DeviceAgentBuilder::Information::Information(
    InformationTypeBypassed,
    std::shared_ptr<nx::vms::server::plugins::HanwhaSharedResourceContext>& context,
    int channelNumber)
{
    plugins::HanwhaRequestHelper nvrHelper(context, channelNumber);

    this->attributes = nvrHelper.fetchAttributes("attributes");
    this->cgiParameters = nvrHelper.fetchCgiParameters("cgis");

    plugins::HanwhaResponse eventStatuses = nvrHelper.check("eventstatus/eventstatus");
    if (eventStatuses.isSuccessful())
        this->eventStatusMap = eventStatuses.response();

    // We can't discover real channel number and real device type for a bypassed device,
    // so we consider channel number to be 0 and device type to be non-NVR.
    this->channelNumber = 0;
    this->isNvr = false;
}

bool DeviceAgentBuilder::Information::isValid() const
{
    return attributes.isValid() && cgiParameters.isValid() && !eventStatusMap.empty();
}

//-------------------------------------------------------------------------------------------------

DeviceAgentBuilder::DeviceAgentBuilder(
    const nx::sdk::IDeviceInfo* deviceInfo,
    Engine* engine,
    std::shared_ptr<nx::vms::server::plugins::HanwhaSharedResourceContext>& context)
    :
    m_deviceInfo(deviceInfo),
    m_engine(engine),
    m_engineManifest(engine->manifest()),
    m_deviceDebugInfo(NX_FMT("%1 (%2)", deviceInfo->name(), deviceInfo->id())),
    m_directInfo(InformationTypeDirect(), context, deviceInfo->channelNumber()),
    m_bypassedInfo(InformationTypeBypassed(), context, deviceInfo->channelNumber())
{
}

//-------------------------------------------------------------------------------------------------

SettingsCapabilities DeviceAgentBuilder::fetchSettingsCapabilities() const
{
    const plugins::HanwhaCgiParameters& cgi =
        m_directInfo.isNvr ? m_bypassedInfo.cgiParameters : m_directInfo.cgiParameters;

    SettingsCapabilities result;
    if (!cgi.isValid())
    {
        NX_DEBUG(this, kFetchErrorPattern,
            "CGI parameters", m_deviceDebugInfo,
            "SettingsCapabilities can not be fetched. Default values will be used. Roi may work wrong.");
        return result;
    }

    // Tampering
    result.tampering.enabled = cgi.parameter("eventsources","tamperingdetection",
        "set", "Enable").has_value();

    result.tampering.thresholdLevel =
        cgi.parameter("eventsources", "tamperingdetection", "set", "ThresholdLevel").has_value();

    result.tampering.sensitivityLevel =
        cgi.parameter("eventsources", "tamperingdetection", "set", "SensitivityLevel").has_value();

    result.tampering.minimumDuration =
        cgi.parameter("eventsources", "tamperingdetection", "set", "Duration").has_value();

    result.tampering.exceptDarkImages =
        cgi.parameter("eventsources", "tamperingdetection", "set", "DarknessDetection").has_value();

    // Defocus
    result.defocus.enabled = cgi.parameter("eventsources","defocusdetection",
        "set", "Enable").has_value();

    result.defocus.thresholdLevel =
        cgi.parameter("eventsources", "defocusdetection", "set", "ThresholdLevel").has_value();

    result.defocus.sensitivityLevel =
        cgi.parameter("eventsources", "defocusdetection", "set", "Sensitivity").has_value();

    result.defocus.minimumDuration =
        cgi.parameter("eventsources", "defocusdetection", "set", "Duration").has_value();

    result.videoAnalysis = cgi.hasSubmenu("eventsources", "videoanalysis");
    result.videoAnalysis2 = cgi.hasSubmenu("eventsources", "videoanalysis2");

    // Line
    result.ivaLine.ruleName =
        cgi.parameter("eventsources", "videoanalysis2", "set",
        "Line.#.RuleName").has_value();

    result.ivaLine.objectTypeFilter =
        cgi.parameter("eventsources", "videoanalysis2", "set",
        "Line.#.ObjectTypeFilter").has_value();

    // Area
    result.ivaArea.ruleName = cgi.parameter("eventsources", "videoanalysis2", "set",
        "DefinedArea.#.RuleName").has_value();
    result.ivaArea.objectTypeFilter = cgi.parameter("eventsources", "videoanalysis2", "set",
        "DefinedArea.#.ObjectTypeFilter").has_value();

    auto mode = cgi.parameter("eventsources", "videoanalysis2", "set",
        "DefinedArea.#.Mode");
    if (mode)
    {
        result.ivaArea.intrusion = mode->possibleValues().contains("Intrusion");
        result.ivaArea.enter = mode->possibleValues().contains("Entering");
        result.ivaArea.exit = mode->possibleValues().contains("Exiting");
        result.ivaArea.appear = mode->possibleValues().contains("AppearDisappear");
        result.ivaArea.loitering = mode->possibleValues().contains("Loitering");
    }
    else
    {
        result.ivaArea.intrusion = false;
        result.ivaArea.enter = false;
        result.ivaArea.exit = false;
        result.ivaArea.appear = false;
        result.ivaArea.loitering = false;
    }

    result.ivaArea.intrusionDuration = cgi.parameter("eventsources", "videoanalysis2", "set",
        "DefinedArea.#.IntrusionDuration").has_value();

    result.ivaArea.appearDuration = cgi.parameter("eventsources", "videoanalysis2", "set",
        "DefinedArea.#.AppearanceDuration").has_value();

    result.ivaArea.loiteringDuration = cgi.parameter("eventsources", "videoanalysis2", "set",
        "DefinedArea.#.LoiteringDuration").has_value();

    // Box temperature
    result.boxTemperature.unnamedRect = cgi.parameter("eventsources",
        "boxtemperaturedetection", "set", "ROI.#.Coordinate").has_value();

    result.boxTemperature.temperatureType = cgi.parameter("eventsources",
        "boxtemperaturedetection", "set", "ROI.#.TemperatureType").has_value();

    result.boxTemperature.detectionType = cgi.parameter("eventsources",
        "boxtemperaturedetection", "set", "ROI.#.DetectionType").has_value();

    result.boxTemperature.thresholdTemperature = cgi.parameter("eventsources",
        "boxtemperaturedetection", "set", "ROI.#.ThresholdTemperature").has_value();

    result.boxTemperature.duration = cgi.parameter("eventsources",
        "boxtemperaturedetection", "set", "ROI.#.Duration").has_value();

    result.boxTemperature.areaEmissivity = cgi.parameter("eventsources",
        "boxtemperaturedetection", "set", "ROI.#.NormalizedEmissivity").has_value();

    //Mask
    result.mask.enabled =
        cgi.parameter("eventsources", "maskdetection", "set", "Enable").has_value();
    result.mask.detectionMode =
        cgi.parameter("eventsources", "maskdetection", "set", "DetectionMode").has_value();
    result.mask.duration =
        cgi.parameter("eventsources", "maskdetection", "set", "Duration").has_value();

    return result;
}
//-------------------------------------------------------------------------------------------------

RoiResolution DeviceAgentBuilder::fetchRoiResolution() const
{
    // Commonly used value. We use it if actual value can not be fetched.
    static const RoiResolution kDefaultRoiResolution(3839, 2159);

    if (!m_directInfo.attributes.isValid())
    {
        NX_DEBUG(this, kFetchErrorPattern,
            "attributes", m_deviceDebugInfo,
            "RoiResolution can not be fetched. Default values will be used. Roi may work wrong.");
        return kDefaultRoiResolution;
    }

    auto roiMaxXOptional = m_directInfo.attributes.attribute<QString>(
        "Eventsource", "ROICoordinate.MaxX", m_directInfo.channelNumber);
    auto roiMaxYOptional = m_directInfo.attributes.attribute<QString>(
        "Eventsource", "ROICoordinate.MaxY", m_directInfo.channelNumber);
    if (!roiMaxXOptional || !roiMaxYOptional)
    {
        NX_DEBUG(this, kFetchErrorPattern,
            "ROICoordinate", m_deviceDebugInfo,
            "RoiResolution can not be fetched. Default values will be used. Roi may work wrong.");
        return kDefaultRoiResolution;
    }

    bool maxXIsValidInt = false;
    bool maxYIsValidInt = false;
    const int width = roiMaxXOptional->toInt(&maxXIsValidInt) + 1;
    const int height = roiMaxYOptional->toInt(&maxYIsValidInt) + 1;
    if (!maxXIsValidInt || !maxYIsValidInt)
    {
        NX_DEBUG(this, kFetchErrorPattern,
            "valid ROICoordinate", m_deviceDebugInfo,
            "RoiResolution can not be fetched. Default values will be used. Roi may work wrong.");
        return kDefaultRoiResolution;
    }

    // RoiResolution::isRotated value is not fetched here. It is fetched just before using.
    return RoiResolution(width, height);
}

//-------------------------------------------------------------------------------------------------

bool DeviceAgentBuilder::fetchIsObjectDetectionSupported() const
{
    // Note: What Hanwha calls "object detection" we at NX usually call "object tracking".

    if (m_directInfo.isNvr)
        return false;

    if (!m_directInfo.attributes.isValid())
    {
        NX_DEBUG(this, kFetchErrorPattern,
            "attributes", m_deviceDebugInfo,
            "ObjectDetection support can not be fetched. We consider it is not supported.");
        return false;
    }

    return m_directInfo.attributes.attribute<bool>(
        "Eventsource", "ObjectDetection", m_directInfo.channelNumber).value_or(false);
}

//-------------------------------------------------------------------------------------------------

/**
 * Monitor event types are taken from cgi parameters. Some of them match event types from
 * the Engine manifest, but some define a whole group of manifest event types.
 * E.g. `ShockDetection` - is manifest event type name, but `QueueEvent` consists of 6 event types.
 */

QStringList DeviceAgentBuilder::fetchEventTypeFamilyNamesInternal(const Information& info) const
{
    QStringList result;
    if (!info.cgiParameters.isValid())
    {
        NX_DEBUG(this, kFetchErrorPattern,
            "CGI parameters", m_deviceDebugInfo,
            "Supported event types can not be fetched. Device Agent can support no events.");
        return result;
    }

    auto supportedEventTypesParameter = info.cgiParameters.parameter(
        "eventstatus/eventstatus/monitor/Channel.#.EventType");

    if (!supportedEventTypesParameter.has_value())
    {
        NX_DEBUG(this, "Supported event types parameter is not initialized for %1",
            m_deviceDebugInfo);
        return result;
    }

    result = supportedEventTypesParameter->possibleValues();
    const auto alarmInputParameter = info.cgiParameters.parameter(
        "eventstatus/eventstatus/monitor/AlarmInput");

    if (alarmInputParameter)
    {
        NX_VERBOSE(this, "Adding %1 to supported event type list for %2",
            alarmInputParameter->name(), m_deviceDebugInfo);

        result.push_back(alarmInputParameter->name());
    }

    return result;
}

//-------------------------------------------------------------------------------------------------

QStringList DeviceAgentBuilder::fetchEventTypeFamilyNames() const
{
    QStringList result = fetchEventTypeFamilyNamesInternal(m_directInfo);
    if (m_directInfo.isNvr)
    {
        QStringList bypassedList = fetchEventTypeFamilyNamesInternal(m_bypassedInfo);
        result = listIntersection(result, bypassedList);
    }
    result.sort(); //< for convenient debug
    return result;
}

//-------------------------------------------------------------------------------------------------

QStringList DeviceAgentBuilder::fetchInternalEventTypeNamesForPopulousFamiliesInternal(
    const Information& info) const
{
    QStringList result;
    if (info.eventStatusMap.empty())
    {
        NX_DEBUG(this, "Can not fetch event statuses from Hanwha shared context "
            "to creating Device Agent for %1. Supported event types can not be fetched.",
            m_deviceDebugInfo);
        return result; //< Empty result means no DeviceAgent will be created.
    }

    QStringList decoratedNames;
    decoratedNames.reserve(info.eventStatusMap.size());
    for (const auto& [eventTypeDecoratedName, status]: info.eventStatusMap)
        decoratedNames.push_back(eventTypeDecoratedName);

    const QString prefix = QString("Channel.") + QString::number(info.channelNumber);
    for (const QString& eventTypeName: decoratedNames)
    {
        if (eventTypeName.startsWith(prefix))
        {
            const QString candidate =
                eventTypeName.mid(prefix.size() + 1 /*one additional symbol is a dot*/);

            if (isPopulousFamily(candidate))
                result.push_back(candidate);
        }
    }

    return result;
}

//-------------------------------------------------------------------------------------------------

QStringList DeviceAgentBuilder::fetchInternalEventTypeNamesForPopulousFamilies() const
{
    QStringList result = fetchInternalEventTypeNamesForPopulousFamiliesInternal(m_directInfo);

    if (m_directInfo.isNvr)
    {
        QStringList bypassedList =
            fetchInternalEventTypeNamesForPopulousFamiliesInternal(m_bypassedInfo);
        result = listIntersection(result, bypassedList);
    }
    result.sort(); //< for convenient debug
    return result;
}

//-------------------------------------------------------------------------------------------------

QString DeviceAgentBuilder::fetchTemperatureChangeEventTypeNameInternal(const Information& info) const
{
    static const QString kTemperatureChangeInternalName = "TemperatureChangeDetection";
    const QString prefix =
        nx::format("Channel.%1.%2", info.channelNumber, kTemperatureChangeInternalName);

    for (const auto& [eventTypeDecoratedName, status]: info.eventStatusMap)
    {
        if (eventTypeDecoratedName.startsWith(prefix))
        {
            return m_engineManifest.eventTypeIdByName(kTemperatureChangeInternalName);
        }
    }
    return {};
}

//-------------------------------------------------------------------------------------------------

QString DeviceAgentBuilder::fetchTemperatureChangeEventTypeName() const
{
    QString result = fetchTemperatureChangeEventTypeNameInternal(m_directInfo);

    if (!result.isEmpty() && m_directInfo.isNvr)
    {
        const QString bypassedResult = fetchTemperatureChangeEventTypeNameInternal(m_bypassedInfo);
        if (bypassedResult != result)
            result = "";
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

QString DeviceAgentBuilder::fetchBoxTemperatureEventTypeNameInternal(const Information& info) const
{
    static const QString kBoxTemperatureInternalName = "BoxTemperatureDetection";
    const QString prefix =
        nx::format("Channel.%1.%2", info.channelNumber, kBoxTemperatureInternalName);

    for (const auto& [eventTypeDecoratedName, status]: info.eventStatusMap)
    {
        if (eventTypeDecoratedName.startsWith(prefix))
        {
            return m_engineManifest.eventTypeIdByName(kBoxTemperatureInternalName);
        }
    }
    return {};
}

//-------------------------------------------------------------------------------------------------

QString DeviceAgentBuilder::fetchBoxTemperatureEventTypeName() const
{
    QString result = fetchBoxTemperatureEventTypeNameInternal(m_directInfo);

    if (!result.isEmpty() && m_directInfo.isNvr)
    {
        const QString bypassedResult = fetchBoxTemperatureEventTypeNameInternal(m_bypassedInfo);
        if (bypassedResult != result)
            result = "";
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

QStringList DeviceAgentBuilder::obtainEventTypeIds(
    const QStringList& eventTypeFamilyNames,
    const QStringList& eventTypeInternalNamesForPopulousFamilies) const
{
    QSet<QString> eventTypeIds;
    for (const QString& eventTypeFamilyName: eventTypeFamilyNames)
    {
        if (isPopulousFamily(eventTypeFamilyName))
        {
            for (const QString& eventTypeInternalName: eventTypeInternalNamesForPopulousFamilies)
            {
                const bool isMatched = eventTypeInternalName.startsWith(eventTypeFamilyName);
                if (isMatched)
                {
                    const QString eventTypeId =
                        m_engineManifest.eventTypeIdByName(eventTypeInternalName);
                    if (!eventTypeId.isNull())
                    {
                        NX_VERBOSE(this, "Adding %1 to supported event type list for %2",
                            eventTypeId, m_deviceDebugInfo);

                        eventTypeIds.insert(eventTypeId);
                    }
                }
            }
        }
        else
        {
            // FamilyName coincides with InternalName, i.e. the eventTypeFamily consists of
            // the only eventType
            const QString eventTypeId = m_engineManifest.eventTypeIdByName(eventTypeFamilyName);
            if (!eventTypeId.isEmpty())
            {
                NX_VERBOSE(this, "Adding %1 to supported event type list for %2",
                    eventTypeId, m_deviceDebugInfo);
                eventTypeIds.insert(eventTypeId);
            }

        }
    }
    QList<QString> result = QList<QString>::fromSet(eventTypeIds);
    result.sort(); //< Has convenient look in debugger.
    return result;
}

//-------------------------------------------------------------------------------------------------

QStringList DeviceAgentBuilder::addBoxTemperatureEventTypeNamesIfNeeded(
    const QStringList& eventTypeIds) const
{
    QStringList result(eventTypeIds);
    if (const QString eventTypeId = fetchBoxTemperatureEventTypeName(); !eventTypeId.isEmpty())
        result.append(eventTypeId);
    if (const QString eventTypeId = fetchTemperatureChangeEventTypeName(); !eventTypeId.isEmpty())
        result.append(eventTypeId);
    return result;
}

//-------------------------------------------------------------------------------------------------

QStringList DeviceAgentBuilder::addObjectDetectionEventTypeNamesIfNeeded(
    const QStringList& eventTypeIds) const
{
    QStringList result(eventTypeIds);
    const bool isObjectDetectionSupported = fetchIsObjectDetectionSupported();

    // TODO: this list should be build as c union of cgiParameters <submenu name="objectdetection">
    // and engine manifest
    static const QStringList kObjectDetectionTypeIds = {
        "nx.hanwha.trackingEvent.Person",
        "nx.hanwha.trackingEvent.Face",
        "nx.hanwha.trackingEvent.Vehicle",
        "nx.hanwha.trackingEvent.LicensePlate"
    };

    if (isObjectDetectionSupported)
        result.append(kObjectDetectionTypeIds);

    return result;
}

//-------------------------------------------------------------------------------------------------

QStringList DeviceAgentBuilder::buildSupportedEventTypeIds() const
{
    const QStringList eventTypeFamilyNames = fixFamilyNames(fetchEventTypeFamilyNames());

    const QStringList eventTypeInternalNamesForPopulousFamilies =
        fetchInternalEventTypeNamesForPopulousFamilies();

    const QStringList eventTypeIds = obtainEventTypeIds(
        eventTypeFamilyNames, eventTypeInternalNamesForPopulousFamilies);

    const QStringList eventTypeIdsWithTemperatureEvent =
        addBoxTemperatureEventTypeNamesIfNeeded(eventTypeIds);

    const QStringList eventTypeIdsWithTrackingEvents =
        addObjectDetectionEventTypeNamesIfNeeded(eventTypeIdsWithTemperatureEvent);

    return eventTypeIdsWithTrackingEvents;
}
//-------------------------------------------------------------------------------------------------

QJsonValue DeviceAgentBuilder::buildSettingsModel(
    bool isObjectDetectionSupported,
    const SettingsCapabilities& settingsCapabilities,
    const QSet<QString>& supportedEventTypeSet) const
{
    return filterJsonObjects(
        m_engineManifest.deviceAgentSettingsModel,
        [&](const QJsonObject& node)
        {
            if (const QString name = node["name"].toString(); !name.isNull())
            {
                if (name == "IVA")
                    return settingsCapabilities.videoAnalysis2;

                if (name == "IVA.Line#.ObjectTypeFilter")
                    return settingsCapabilities.ivaLine.objectTypeFilter;
                if (name == "IVA.IncludeArea#.ObjectTypeFilter")
                    return settingsCapabilities.ivaArea.objectTypeFilter;

                if (name == "IVA.IncludeArea#.Intrusion")
                    return settingsCapabilities.ivaArea.intrusion;
                if (name == "IVA.IncludeArea#.Enter")
                    return settingsCapabilities.ivaArea.enter;
                if (name == "IVA.IncludeArea#.Exit")
                    return settingsCapabilities.ivaArea.exit;
                if (name == "IVA.IncludeArea#.Appear")
                    return settingsCapabilities.ivaArea.appear;
                if (name == "IVA.IncludeArea#.Loitering")
                    return settingsCapabilities.ivaArea.loitering;

                if (name == "IVA.IncludeArea#.IntrusionDuration")
                    return settingsCapabilities.ivaArea.intrusionDuration;
                if (name == "IVA.IncludeArea#.AppearDuration")
                    return settingsCapabilities.ivaArea.appearDuration;
                if (name == "IVA.IncludeArea#.LoiteringDuration")
                    return settingsCapabilities.ivaArea.loiteringDuration;
            }

            bool keepByDefault = true;

            bool keepForObjectTracking = false;
            if (const auto required = node["_requiredForObjectDetection"]; required.toBool())
            {
                keepByDefault = false;
                keepForObjectTracking = isObjectDetectionSupported;
            }

            bool keepForEvents = false;
            if (const auto eventTypeIds = node["_requiredForEventTypeIds"].toArray();
                !eventTypeIds.empty())
            {
                keepByDefault = false;
                for (const auto eventTypeId : eventTypeIds)
                {
                    if (supportedEventTypeSet.contains(eventTypeId.toString()))
                    {
                        keepForEvents = true;
                        break;
                    }
                }
            }

            return keepByDefault || keepForObjectTracking || keepForEvents;
        });
}

//-------------------------------------------------------------------------------------------------

QStringList DeviceAgentBuilder::buildSupportedObjectTypeIds() const
{
    QStringList result;

    const bool isObjectDetectionSupported = fetchIsObjectDetectionSupported();
    if (isObjectDetectionSupported)
    {
        // DeviceAgent should understand all engine's object types.
        result.reserve(m_engineManifest.objectTypes.size());

        // TODO: only supported objectTypes should be added (may be checked via cgiParameters)
        for (const nx::vms::api::analytics::ObjectType& objectType: m_engineManifest.objectTypes)
            result.push_back(objectType.id);
    }
    else
    {
        NX_DEBUG(this, "Device %1: doesn't support object detection/tracking.",
            m_deviceDebugInfo);
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

Hanwha::DeviceAgentManifest DeviceAgentBuilder::buildDeviceAgentManifest(
    const SettingsCapabilities& settingsCapabilities) const
{
    Hanwha::DeviceAgentManifest deviceAgentManifest;

    deviceAgentManifest.supportedObjectTypeIds = buildSupportedObjectTypeIds();

    deviceAgentManifest.supportedEventTypeIds = buildSupportedEventTypeIds();

    const auto supportedEventTypeSet = QSet<QString>::fromList(
        deviceAgentManifest.supportedEventTypeIds);

    deviceAgentManifest.deviceAgentSettingsModel = buildSettingsModel(
        !deviceAgentManifest.supportedObjectTypeIds.isEmpty(), settingsCapabilities, supportedEventTypeSet);

    return deviceAgentManifest;
}

//-------------------------------------------------------------------------------------------------

std::unique_ptr<DeviceAgent> DeviceAgentBuilder::createDeviceAgent() const
{
    if (!m_directInfo.isValid() || (m_directInfo.isNvr && !m_bypassedInfo.isValid()))
    {
        NX_DEBUG(this, "Can not fetch device information from Hanwha shared context "
            "to creating Device Agent for %1. Creation fail.",
            m_deviceDebugInfo);
        return nullptr;
    }

    const SettingsCapabilities settingsCapabilities = fetchSettingsCapabilities();
    const RoiResolution roiReolution = fetchRoiResolution();
    const Hanwha::DeviceAgentManifest deviceAgentManifest = buildDeviceAgentManifest(settingsCapabilities);

    return std::make_unique<DeviceAgent>(
        m_deviceInfo,
        m_engine,
        settingsCapabilities,
        roiReolution,
        m_directInfo.isNvr,
        deviceAgentManifest);
}
//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
