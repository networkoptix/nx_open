// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_resource.h"

#include <QtCore/QUrlQuery>

#include <common/common_module.h>
#include <core/resource/camera_media_stream_info.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/build_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/kit/utils.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/crypt/symmetrical.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/math/math.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <utils/common/util.h>

#include "media_server_resource.h"
#include "resource_data.h"
#include "resource_data_structures.h"
#include "resource_property_key.h"

namespace {

bool storeUrlForRole(Qn::ConnectionRole role)
{
    switch (role)
    {
        case Qn::CR_LiveVideo:
        case Qn::CR_SecondaryLiveVideo:
            return true;
        default:
            break;
    }
    return false;
}

} // namespace

using AnalyzedStreamIndexMap = QMap<QnUuid, nx::vms::api::StreamIndex>;
using AnalyticsEntitiesByEngine = QnVirtualCameraResource::AnalyticsEntitiesByEngine;

const QString QnVirtualCameraResource::kCompatibleAnalyticsEnginesProperty(
    "compatibleAnalyticsEngines");

const QString QnVirtualCameraResource::kUserEnabledAnalyticsEnginesProperty(
    "userEnabledAnalyticsEngines");

const QString QnVirtualCameraResource::kDeviceAgentsSettingsValuesProperty(
    "deviceAgentsSettingsValuesProperty");

const QString QnVirtualCameraResource::kDeviceAgentManifestsProperty(
    "deviceAgentManifests");

const QString QnVirtualCameraResource::kAnalyzedStreamIndexes(
    "analyzedStreamIndexes");

const QString QnVirtualCameraResource::kVirtualCameraIgnoreTimeZone(
    "virtualCameraIgnoreTimeZone");

const QString QnVirtualCameraResource::kHttpPortParameterName(
    "http_port");

const QString QnVirtualCameraResource::kCameraNameParameterName(
    "name");

const QString QnVirtualCameraResource::kUsingOnvifMedia2Type(
    "usingOnvifMedia2Type");

QnVirtualCameraResource::QnVirtualCameraResource():
    base_type(),
    m_cachedUserEnabledAnalyticsEngines(
        [this]() { return calculateUserEnabledAnalyticsEngines(); }),
    m_cachedCompatibleAnalyticsEngines(
        [this]() { return calculateCompatibleAnalyticsEngines(); }),
    m_cachedDeviceAgentManifests(
        [this]() { return fetchDeviceAgentManifests(); }),
    m_cachedSupportedEventTypes(
        [this]() { return calculateSupportedEventTypes(); }),
    m_cachedSupportedObjectTypes(
        [this]() { return calculateSupportedObjectTypes(); })
{
}

bool QnVirtualCameraResource::isForcedAudioSupported() const
{
    QString val = getProperty(ResourcePropertyKey::kForcedIsAudioSupported);
    return val.toUInt() > 0;
}

void QnVirtualCameraResource::forceEnableAudio()
{
    if (isForcedAudioSupported())
        return;
    setProperty(ResourcePropertyKey::kForcedIsAudioSupported, "1");
    saveProperties();
}

void QnVirtualCameraResource::forceDisableAudio()
{
    if (!isForcedAudioSupported())
        return;
    setProperty(ResourcePropertyKey::kForcedIsAudioSupported, "0");
    saveProperties();
}

void QnVirtualCameraResource::updateDefaultAuthIfEmpty(const QString& login, const QString& password)
{
    auto credentials = getProperty(ResourcePropertyKey::kDefaultCredentials);

    if (credentials.isEmpty() || credentials == ":")
    {
        setDefaultAuth(login, password);
        saveProperties();
    }
}

QString QnVirtualCameraResource::sourceUrl(Qn::ConnectionRole role) const
{
    if (!storeUrlForRole(role))
        return QString();

    QJsonObject streamUrls =
        QJsonDocument::fromJson(getProperty(ResourcePropertyKey::kStreamUrls).toUtf8()).object();
    const auto roleStr = QString::number(role);
    return streamUrls[roleStr].toString();
}

void QnVirtualCameraResource::updateSourceUrl(const nx::utils::Url& tempUrl,
    Qn::ConnectionRole role,
    bool save)
{
    QString url(tempUrl.toString());

    NX_VERBOSE(this, nx::format("Update %1 stream %2 URL: %3").args(getPhysicalId(), role, url));
    if (!storeUrlForRole(role))
        return;

    NX_DEBUG(this, nx::format("Save %1 stream %2 URL: %3").args(getPhysicalId(), role, url));

    const auto roleStr = QString::number(role);
    bool urlChanged = false;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        QJsonObject streamUrls = QJsonDocument::fromJson(
            getProperty(ResourcePropertyKey::kStreamUrls).toUtf8()).object();
        streamUrls[roleStr] = url;
        urlChanged = setProperty(ResourcePropertyKey::kStreamUrls,
            QString::fromUtf8(QJsonDocument(streamUrls).toJson()));
    }

    //TODO: #rvasilenko Setter and saving must be split.
    if (urlChanged && save)
        saveProperties();
}

bool QnVirtualCameraResource::virtualCameraIgnoreTimeZone() const
{
    return QnLexical::deserialized(
        getProperty(kVirtualCameraIgnoreTimeZone),
        false);
}

void QnVirtualCameraResource::setVirtualCameraIgnoreTimeZone(bool value)
{
    NX_ASSERT(hasFlags(Qn::virtual_camera));
    setProperty(kVirtualCameraIgnoreTimeZone, value ? "true" : "");
}

nx::vms::api::RtpTransportType QnVirtualCameraResource::preferredRtpTransport() const
{
    return nx::reflect::fromString(
        getProperty(QnMediaResource::rtpTransportKey()).toStdString(),
        nx::vms::api::RtpTransportType::automatic);
}

CameraMediaStreams QnVirtualCameraResource::mediaStreams() const
{
    const QString& mediaStreamsStr = getProperty(ResourcePropertyKey::kMediaStreams);
    CameraMediaStreams supportedMediaStreams = QJson::deserialized<CameraMediaStreams>(mediaStreamsStr.toLatin1());

    // TODO #lbusygin: just for vms_4.2 version, get rid of this code when the mobile client
    // stops supporting servers < 5.0.
    for(auto& str: supportedMediaStreams.streams)
    {
        if (str.codec == AV_CODEC_ID_INDEO3)
            str.codec = AV_CODEC_ID_H264;
        if (str.codec == AV_CODEC_ID_FIC)
            str.codec = AV_CODEC_ID_H265;
    }
    return supportedMediaStreams;
}

CameraMediaStreamInfo QnVirtualCameraResource::streamInfo(StreamIndex index) const
{
    const auto streams = mediaStreams().streams;
    auto stream = std::find_if(streams.cbegin(), streams.cend(),
        [index](const CameraMediaStreamInfo& stream)
        {
            return stream.getEncoderIndex() == index;
        });
    if (stream != streams.cend())
        return *stream;

    return CameraMediaStreamInfo();
}

QnAspectRatio QnVirtualCameraResource::aspectRatio() const
{
    if (auto aspectRatio = customAspectRatio(); aspectRatio.isValid())
        return aspectRatio;

    // The rest of the code deals with auto aspect ratio.
    // Aspect ration should be forced to AS of the first stream. Note: primary stream AR could be
    // changed on the fly and a camera may not have a primary stream. In this case natural
    // secondary stream AS should be used.
    const auto stream = streamInfo(StreamIndex::primary);
    QSize size = stream.getResolution();
    if (size.isEmpty())
    {
        // Trying to use size from secondary stream
        const auto secondary = streamInfo(StreamIndex::secondary);
        size = secondary.getResolution();
    }

    if (size.isEmpty())
        return QnAspectRatio();

    return QnAspectRatio(size);
}

QnAspectRatio QnVirtualCameraResource::aspectRatioRotated() const
{
    return QnAspectRatio::isRotated90(forcedRotation().value_or(0))
        ? aspectRatio().inverted()
        : aspectRatio();
}

void QnVirtualCameraResource::emitPropertyChanged(
    const QString& key, const QString& prevValue, const QString& newValue)
{
    if (key == ResourcePropertyKey::kPtzCapabilities)
        emit ptzCapabilitiesChanged(::toSharedPointer(this));

    if (key == kUserEnabledAnalyticsEnginesProperty)
    {
        m_cachedUserEnabledAnalyticsEngines.reset();
        m_cachedSupportedEventTypes.reset();
        m_cachedSupportedObjectTypes.reset();
        emit userEnabledAnalyticsEnginesChanged(toSharedPointer(this));
        emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
        emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
    }

    if (key == kCompatibleAnalyticsEnginesProperty)
    {
        m_cachedCompatibleAnalyticsEngines.reset();
        m_cachedSupportedEventTypes.reset();
        m_cachedSupportedObjectTypes.reset();
        emit compatibleAnalyticsEnginesChanged(toSharedPointer(this));
        emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
        emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
    }

    if (key == kDeviceAgentManifestsProperty)
    {
        m_cachedDeviceAgentManifests.reset();
        m_cachedSupportedEventTypes.reset();
        m_cachedSupportedObjectTypes.reset();
        emit deviceAgentManifestsChanged(toSharedPointer(this));
        emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
        emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
    }

    if (key == ResourcePropertyKey::kIoConfigCapability)
    {
        emit isIOModuleChanged(::toSharedPointer(this));
    }

    if (key == kDeviceAgentManifestsProperty || key == kAnalyzedStreamIndexes)
    {
        auto cachedAnalyzedStreamIndex = m_cachedAnalyzedStreamIndex.lock();
        cachedAnalyzedStreamIndex->clear();
    }

    base_type::emitPropertyChanged(key, prevValue, newValue);
}

QSet<QnUuid> QnVirtualCameraResource::enabledAnalyticsEngines() const
{
    return nx::utils::toQSet(enabledAnalyticsEngineResources().ids());
}

const nx::vms::common::AnalyticsEngineResourceList
    QnVirtualCameraResource::enabledAnalyticsEngineResources() const
{
    const auto selectedEngines = userEnabledAnalyticsEngines();
    return compatibleAnalyticsEngineResources().filtered(
        [selectedEngines](const nx::vms::common::AnalyticsEngineResourcePtr& engine)
        {
            return engine->isDeviceDependent()
                || selectedEngines.contains(engine->getId());
        });
}

QSet<QnUuid> QnVirtualCameraResource::userEnabledAnalyticsEngines() const
{
    return m_cachedUserEnabledAnalyticsEngines.get();
}

void QnVirtualCameraResource::setUserEnabledAnalyticsEngines(const QSet<QnUuid>& engines)
{
    const auto p = serializeUserEnabledAnalyticsEngines(engines);
    setProperty(p.name, p.value);
}

nx::vms::api::ResourceParamData QnVirtualCameraResource::serializeUserEnabledAnalyticsEngines(
    const QSet<QnUuid>& engines)
{
    return nx::vms::api::ResourceParamData(
        kUserEnabledAnalyticsEnginesProperty,
        QString::fromUtf8(QJson::serialized(engines))
    );
}

const QSet<QnUuid> QnVirtualCameraResource::compatibleAnalyticsEngines() const
{
    return m_cachedCompatibleAnalyticsEngines.get();
}

nx::vms::common::AnalyticsEngineResourceList
    QnVirtualCameraResource::compatibleAnalyticsEngineResources() const
{
    const auto resPool = resourcePool();
    if (!resPool)
        return {};

    return resPool->getResourcesByIds<nx::vms::common::AnalyticsEngineResource>(
        compatibleAnalyticsEngines());
}

void QnVirtualCameraResource::setCompatibleAnalyticsEngines(const QSet<QnUuid>& engines)
{
    auto ensureEnginesAreActive =
        [this, engines]
        {
            const auto server = getParentServer();
            if (!server)
                return false;

            // Make sure all set engines are active.
            return (engines - server->activeAnalyticsEngineIds()).empty();
        };

    //NX_ASSERT_HEAVY_CONDITION(ensureEnginesAreActive());
    setProperty(kCompatibleAnalyticsEnginesProperty, QString::fromUtf8(QJson::serialized(engines)));
}

AnalyticsEntitiesByEngine QnVirtualCameraResource::filterByEngineIds(
    AnalyticsEntitiesByEngine entitiesByEngine,
    const QSet<QnUuid>& engineIds)
{
    for (auto it = entitiesByEngine.cbegin(); it != entitiesByEngine.cend();)
    {
        const auto engineId = it->first;
        if (engineIds.contains(engineId))
            ++it;
        else
            it = entitiesByEngine.erase(it);
    }

    return entitiesByEngine;
}

AnalyticsEntitiesByEngine QnVirtualCameraResource::supportedEventTypes() const
{
    return filterByEngineIds(m_cachedSupportedEventTypes.get(), enabledAnalyticsEngines());
}

AnalyticsEntitiesByEngine QnVirtualCameraResource::supportedObjectTypes(
    bool filterByEngines) const
{
    return filterByEngines
        ? filterByEngineIds(m_cachedSupportedObjectTypes.get(), enabledAnalyticsEngines())
        : m_cachedSupportedObjectTypes.get();
}

QSet<QnUuid> QnVirtualCameraResource::calculateUserEnabledAnalyticsEngines() const
{
    return calculateUserEnabledAnalyticsEngines(getProperty(kUserEnabledAnalyticsEnginesProperty));
}

QSet<QnUuid> QnVirtualCameraResource::calculateUserEnabledAnalyticsEngines(const QString& value)
{
    return QJson::deserialized<QSet<QnUuid>>(value.toUtf8());
}

QSet<QnUuid> QnVirtualCameraResource::calculateCompatibleAnalyticsEngines() const
{
    return QJson::deserialized<QSet<QnUuid>>(
        getProperty(kCompatibleAnalyticsEnginesProperty).toUtf8());
}

AnalyticsEntitiesByEngine QnVirtualCameraResource::calculateSupportedEntities(
     ManifestItemIdsFetcher fetcher) const
{
    AnalyticsEntitiesByEngine result;
    auto deviceAgentManifests = m_cachedDeviceAgentManifests.get();
    for (const auto& [engineId, deviceAgentManifest]: deviceAgentManifests)
        result[engineId] = fetcher(deviceAgentManifest);

    return result;
}

AnalyticsEntitiesByEngine QnVirtualCameraResource::calculateSupportedEventTypes() const
{
    return calculateSupportedEntities(
        [](const nx::vms::api::analytics::DeviceAgentManifest& deviceAgentManifest)
        {
            std::set<QString> result;
            result.insert(
                deviceAgentManifest.supportedEventTypeIds.cbegin(),
                deviceAgentManifest.supportedEventTypeIds.cend());

            for (const auto& eventType: deviceAgentManifest.eventTypes)
                result.insert(eventType.id);

            for (const auto& supportedType: deviceAgentManifest.supportedTypes)
            {
                if (supportedType.objectTypeId.isEmpty() && !supportedType.eventTypeId.isEmpty())
                    result.insert(supportedType.objectTypeId);
            }

            return result;
        });
}

AnalyticsEntitiesByEngine QnVirtualCameraResource::calculateSupportedObjectTypes() const
{
    return calculateSupportedEntities(
        [](const nx::vms::api::analytics::DeviceAgentManifest& deviceAgentManifest)
        {
            std::set<QString> result;
            result.insert(
                deviceAgentManifest.supportedObjectTypeIds.cbegin(),
                deviceAgentManifest.supportedObjectTypeIds.cend());

            for (const auto& objectType: deviceAgentManifest.objectTypes)
                result.insert(objectType.id);

            for (const auto& supportedType: deviceAgentManifest.supportedTypes)
            {
                if (!supportedType.objectTypeId.isEmpty() && supportedType.eventTypeId.isEmpty())
                    result.insert(supportedType.objectTypeId);
            }

            return result;
        });
}

QnVirtualCameraResource::DeviceAgentManifestMap
    QnVirtualCameraResource::fetchDeviceAgentManifests() const
{
    const auto data = getProperty(kDeviceAgentManifestsProperty).toStdString();
    if (data.empty())
        return {};

    auto [deserializedManifestMap, result] =
        nx::reflect::json::deserialize<DeviceAgentManifestMap>(data);
    if (!result)
        NX_WARNING(this, "Failed to deserialize manifest: %1", result.toString());
    return deserializedManifestMap;
}

std::optional<nx::vms::api::analytics::DeviceAgentManifest>
    QnVirtualCameraResource::deviceAgentManifest(const QnUuid& engineId) const
{
    const auto manifests = m_cachedDeviceAgentManifests.get();
    auto it = manifests.find(engineId);
    if (it == manifests.cend())
        return std::nullopt;

    return it->second;
}

void QnVirtualCameraResource::setDeviceAgentManifest(
    const QnUuid& engineId,
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    auto manifests = m_cachedDeviceAgentManifests.get();
    manifests[engineId] = manifest;

    setProperty(
        kDeviceAgentManifestsProperty,
        QString::fromUtf8(nx::reflect::json::serialize(manifests)));
    m_cachedDeviceAgentManifests.reset();
}

std::optional<nx::vms::api::StreamIndex>
    QnVirtualCameraResource::obtainUserChosenAnalyzedStreamIndex(QnUuid engineId) const
{
    using nx::vms::api::analytics::DeviceAgentManifest;

    const std::optional<DeviceAgentManifest> deviceAgentManifest =
        this->deviceAgentManifest(engineId);
    if (!deviceAgentManifest)
        return std::nullopt;

    if (deviceAgentManifest->capabilities.testFlag(
        DeviceAgentManifest::Capability::disableStreamSelection))
    {
        return std::nullopt;
    }

    const QString serializedProperty = getProperty(kAnalyzedStreamIndexes);
    if (serializedProperty.isEmpty())
        return std::nullopt;

    bool success = false;
    const auto analyzedStreamIndexMap =
        QJson::deserialized(serializedProperty.toUtf8(), AnalyzedStreamIndexMap(), &success);
    if (!success)
    {
        NX_WARNING(this,
            "%1 Unable to deserialize the analyzedStreamIndex map for the Device %2 (%3), "
            "\"%4\" property content: %5",
            __func__, getUserDefinedName(), getId(), kAnalyzedStreamIndexes,
            nx::kit::utils::toString(serializedProperty));
        return kDefaultAnalyzedStreamIndex;
    }

    if (const auto it = analyzedStreamIndexMap.find(engineId); it != analyzedStreamIndexMap.end())
        return *it;

    return std::nullopt;
}

nx::vms::api::StreamIndex QnVirtualCameraResource::analyzedStreamIndex(QnUuid engineId) const
{
    auto cachedAnalyzedStreamIndex = m_cachedAnalyzedStreamIndex.lock();
    auto it = cachedAnalyzedStreamIndex->find(engineId);
    if (it != cachedAnalyzedStreamIndex->end())
        return it->second;
    auto result = analyzedStreamIndexInternal(engineId);
    cachedAnalyzedStreamIndex->emplace(engineId, result);
    return result;
}

nx::vms::api::StreamIndex QnVirtualCameraResource::analyzedStreamIndexInternal(const QnUuid& engineId) const
{
    const std::optional<nx::vms::api::StreamIndex> userChosenAnalyzedStreamIndex =
        obtainUserChosenAnalyzedStreamIndex(engineId);
    if (userChosenAnalyzedStreamIndex)
        return userChosenAnalyzedStreamIndex.value();

    const QnResourcePool* const resourcePool = this->resourcePool();
    if (!NX_ASSERT(resourcePool))
        return kDefaultAnalyzedStreamIndex;

    const auto engineResource = resourcePool->getResourceById(
        engineId).dynamicCast<nx::vms::common::AnalyticsEngineResource>();
    if (!engineResource)
        return kDefaultAnalyzedStreamIndex;

    const nx::vms::api::analytics::EngineManifest manifest = engineResource->manifest();
    if (manifest.preferredStream != nx::vms::api::StreamIndex::undefined)
        return manifest.preferredStream;

    return kDefaultAnalyzedStreamIndex;
}

void QnVirtualCameraResource::setAnalyzedStreamIndex(
    QnUuid engineId, nx::vms::api::StreamIndex streamIndex)
{
    const QString serializedProperty = getProperty(kAnalyzedStreamIndexes);

    bool success = false;
    auto analyzedStreamIndexMap = QJson::deserialized(
        serializedProperty.toUtf8(), AnalyzedStreamIndexMap(), &success);

    if (!success && !serializedProperty.isEmpty())
    {
        NX_WARNING(this,
            "%1 Unable to deserialize the analyzedStreamIndex map for the Device %2 (%3), "
            "\"%4\" property content: %5",
            __func__, getUserDefinedName(), getId(), kAnalyzedStreamIndexes, serializedProperty);
    }

    analyzedStreamIndexMap[engineId] = streamIndex;

    setProperty(
        kAnalyzedStreamIndexes,
        QString::fromUtf8(QJson::serialized(analyzedStreamIndexMap)));
}

bool QnVirtualCameraResource::hasDualStreamingInternal() const
{
    // Calculate secondary stream capability for manually added based on the custom streams.
    const auto supportsUserDefinedStreams =
        hasCameraCapabilities(nx::vms::api::DeviceCapability::customMediaUrl);
    if (supportsUserDefinedStreams)
    {
        const auto primaryStream = sourceUrl(Qn::CR_LiveVideo);
        const auto secondaryStream = sourceUrl(Qn::CR_SecondaryLiveVideo);
        if (!primaryStream.isEmpty() && !secondaryStream.isEmpty())
            return true;
    }

    return base_type::hasDualStreamingInternal();
}

bool QnVirtualCameraResource::isWebPageSupported() const
{
    if (hasFlags(Qn::virtual_camera))
        return false;

    const bool isNetworkLink = hasCameraCapabilities(
        nx::vms::api::DeviceCapabilities(nx::vms::api::DeviceCapability::customMediaUrl)
            | nx::vms::api::DeviceCapability::fixedQuality);
    if (isNetworkLink)
        return false;

    const bool isUsbDevice = getVendor() == "usb_cam"
        && getMAC().isNull()
        && sourceUrl(Qn::CR_LiveVideo).isEmpty()
        && sourceUrl(Qn::CR_SecondaryLiveVideo).isEmpty();
    if (isUsbDevice)
        return false;

    return true;
}

int QnVirtualCameraResource::customWebPagePort() const
{
    return getProperty(QnVirtualCameraResource::kHttpPortParameterName).toInt();
}

void QnVirtualCameraResource::setCustomWebPagePort(int value)
{
    if (!NX_ASSERT(value >= 0 && value < 65536, "Port number is invalid"))
        return;

    const QString propertyValue = value > 0 ? QString::number(value) : QString();
    setProperty(QnVirtualCameraResource::kHttpPortParameterName, propertyValue);
}

Qn::RecordingState QnVirtualCameraResource::recordingState() const
{
    if (!isScheduleEnabled())
        return Qn::RecordingState::Off;
    if (getStatus() == nx::vms::api::ResourceStatus::recording)
        return Qn::RecordingState::On;
    return Qn::RecordingState::Scheduled;
}

bool QnVirtualCameraResource::isMotionDetectionActive() const
{
    if (!isMotionDetectionEnabled())
        return false;

    if (getStatus() == nx::vms::api::ResourceStatus::unauthorized)
        return false;

    if (getMotionType() != MotionType::software)
        return true;

    const auto motionStream = motionStreamIndex();
    if (motionStream.isForced)
        return true;

    auto streamIndex = nx::vms::api::StreamIndex::undefined;
    if (!hasDualStreaming())
    {
        streamIndex = nx::vms::api::StreamIndex::primary;
    }
    else
    {
        streamIndex = motionStream.index != nx::vms::api::StreamIndex::undefined
            ? motionStream.index
            : nx::vms::api::StreamIndex::secondary;
    }

    const auto streamResolution = streamInfo(streamIndex).getResolution();
    const auto pixelCount = streamResolution.width() * streamResolution.height();

    return pixelCount <= QnVirtualCameraResource::kMaximumMotionDetectionPixels;
}

bool QnVirtualCameraResource::canApplyScheduleTask(
    const QnScheduleTask& task,
    bool dualStreamingAllowed,
    bool motionDetectionAllowed,
    bool objectDetectionAllowed)
{
    using namespace nx::vms::api;

    if (task.recordingType == RecordingType::always || task.recordingType == RecordingType::never)
        return true;

    if (task.recordingType == RecordingType::metadataAndLowQuality && !dualStreamingAllowed)
        return false;

    if (task.metadataTypes.testFlag(RecordingMetadataType::motion) && !motionDetectionAllowed)
        return false;

    if (task.metadataTypes.testFlag(RecordingMetadataType::objects) && !objectDetectionAllowed)
        return false;

    return true;
}

bool QnVirtualCameraResource::canApplyScheduleTask(const QnScheduleTask& task) const
{
    return canApplyScheduleTask(task,
        hasDualStreaming() && isSecondaryStreamRecorded(),
        isMotionDetectionActive(),
        !supportedObjectTypes().empty());
}

bool QnVirtualCameraResource::canApplySchedule(
    const QnScheduleTaskList& schedule,
    bool dualStreamingAllowed,
    bool motionDetectionAllowed,
    bool objectDetectionAllowed)
{
    return std::all_of(schedule.cbegin(), schedule.cend(),
        [&](const QnScheduleTask& task)
        {
            return canApplyScheduleTask(
                task,
                dualStreamingAllowed,
                motionDetectionAllowed,
                objectDetectionAllowed);
        });
}

bool QnVirtualCameraResource::canApplySchedule(const QnScheduleTaskList& schedule) const
{
    return canApplySchedule(schedule,
        hasDualStreaming() && isSecondaryStreamRecorded(),
        isMotionDetectionActive(),
        !supportedObjectTypes().empty());
}

void QnVirtualCameraResource::setAvailableProfiles(const nx::vms::api::DeviceProfiles& value)
{
    setProperty(ResourcePropertyKey::kAvailableProfiles,
        QString::fromStdString(nx::reflect::json::serialize(value)));
}

nx::vms::api::DeviceProfiles QnVirtualCameraResource::availableProfiles() const
{
    const auto data = getProperty(ResourcePropertyKey::kAvailableProfiles).toStdString();
    if (data.empty())
        return {};

    auto [profiles, result] = nx::reflect::json::deserialize<nx::vms::api::DeviceProfiles>(data);
    if (!result)
    {
        NX_WARNING(this, "Failed to deserialize available profiles for device %1: %2",
            getId(), result.toString());
    }
    return profiles;
}

void QnVirtualCameraResource::setForcedProfile(const QString& token, nx::vms::api::StreamIndex index)
{
    using namespace nx::vms::api;
    setProperty(index == StreamIndex::primary
        ? ResourcePropertyKey::kForcedPrimaryProfile
        : ResourcePropertyKey::kForcedSecondaryProfile,
        token);
}

QString QnVirtualCameraResource::forcedProfile(nx::vms::api::StreamIndex index) const
{
    return getProperty(
        index == StreamIndex::primary
        ? ResourcePropertyKey::kForcedPrimaryProfile
        : ResourcePropertyKey::kForcedSecondaryProfile);
}
