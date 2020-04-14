#include "camera_resource.h"

#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>

#include <nx/vms/api/data/camera_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_camera_manager.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource/resource_data_structures.h>

#include <nx_ec/ec_api.h>
#include <nx/fusion/model_functions.h>
#include <utils/common/util.h>
#include <utils/crypt/symmetrical.h>
#include <utils/math/math.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/app_info.h>
#include <nx/fusion/model_functions.h>

#include <nx/kit/utils.h>

namespace {

static const int MAX_ISSUE_CNT = 3; // max camera issues during a period.
static const qint64 ISSUE_KEEP_TIMEOUT_MS = 1000 * 60;
static const qint64 UPDATE_BITRATE_TIMEOUT_DAYS = 7;

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

std::map<QnUuid, std::set<QString>> filterByActiveEngines(
    std::map<QnUuid, std::set<QString>> entitiesByEngine,
    const QSet<QnUuid>& activeEngines)
{
    for (auto it = entitiesByEngine.cbegin(); it != entitiesByEngine.cend();)
    {
        const auto engineId = it->first;
        if (activeEngines.contains(engineId))
            ++it;
        else
            it = entitiesByEngine.erase(it);
    }

    return entitiesByEngine;
}

} // namespace

using AnalyzedStreamIndexMap = QMap<QnUuid, nx::vms::api::StreamIndex>;

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

const QString QnVirtualCameraResource::kWearableIgnoreTimeZone(
    "wearableIgnoreTimeZone");

QnVirtualCameraResource::QnVirtualCameraResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_issueCounter(0),
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
    connect(
        this,
        &QnResource::propertyChanged,
        this,
        [&](auto& resource, auto& key)
        {
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
        });
}

QString QnVirtualCameraResource::getUniqueId() const
{
    return getPhysicalId();
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
    setProperty(ResourcePropertyKey::kForcedIsAudioSupported, 1);
    saveProperties();
}

void QnVirtualCameraResource::forceDisableAudio()
{
    if (!isForcedAudioSupported())
        return;
    setProperty(ResourcePropertyKey::kForcedIsAudioSupported, QString(lit("0")));
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

    NX_VERBOSE(this, lm("Update %1 stream %2 URL: %3").args(getPhysicalId(), role, url));
    if (!storeUrlForRole(role))
        return;

    {
        QnMutexLocker lock(&m_mutex);
        auto cachedUrl = m_cachedStreamUrls.find(role);
        bool cachedUrlExists = cachedUrl != m_cachedStreamUrls.end();

        if (cachedUrlExists && cachedUrl->second == url)
            return;
    }

    auto urlUpdater =
        [url, role](QString oldValue)
        {
            const auto roleStr = QString::number(role);
            QJsonObject streamUrls = QJsonDocument::fromJson(oldValue.toUtf8()).object();
            streamUrls[roleStr] = url;
            return QString::fromUtf8(QJsonDocument(streamUrls).toJson());
        };

    NX_DEBUG(this, lm("Save %1 stream %2 URL: %3").args(getPhysicalId(), role, url));
    if (updateProperty(ResourcePropertyKey::kStreamUrls, urlUpdater))
    {
        //TODO: #rvasilenko Setter and saving must be split.
        if (save)
            saveProperties();
    }
}

int QnVirtualCameraResource::saveAsync()
{
    nx::vms::api::CameraData apiCamera;
    ec2::fromResourceToApi(toSharedPointer(this), apiCamera);

    ec2::AbstractECConnectionPtr conn = commonModule()->ec2Connection();
    return conn->getCameraManager(Qn::kSystemAccess)->addCamera(apiCamera, this, []{});
}

void QnVirtualCameraResource::issueOccured() {
    bool tooManyIssues = false;
    {
        /* Calculate how many issues have occurred during last check period. */
        QnMutexLocker lock( &m_mutex );
        m_issueCounter++;
        tooManyIssues = m_issueCounter >= MAX_ISSUE_CNT;
        m_lastIssueTimer.restart();
    }
    if (tooManyIssues && !hasStatusFlags(Qn::CameraStatusFlag::CSF_HasIssuesFlag)) {
        addStatusFlags(Qn::CameraStatusFlag::CSF_HasIssuesFlag);
        saveAsync();
    }
}

void QnVirtualCameraResource::cleanCameraIssues() {
    {
        /* Check if no issues occurred during last check period. */
        QnMutexLocker lock( &m_mutex );
        if (!m_lastIssueTimer.hasExpired(issuesTimeoutMs()))
            return;
        m_issueCounter = 0;
    }
    if (hasStatusFlags(Qn::CameraStatusFlag::CSF_HasIssuesFlag)) {
        removeStatusFlags(Qn::CameraStatusFlag::CSF_HasIssuesFlag);
        saveAsync();
    }
}

bool QnVirtualCameraResource::wearableIgnoreTimeZone() const
{
    return QnLexical::deserialized(
        getProperty(kWearableIgnoreTimeZone),
        false);
}

void QnVirtualCameraResource::setWearableIgnoreTimeZone(bool value)
{
    NX_ASSERT(hasFlags(Qn::wearable_camera));
    setProperty(kWearableIgnoreTimeZone, value ? true : QVariant());
}

int QnVirtualCameraResource::issuesTimeoutMs() {
    return ISSUE_KEEP_TIMEOUT_MS;
}

nx::vms::api::RtpTransportType QnVirtualCameraResource::preferredRtpTransport() const
{
    return QnLexical::deserialized(
        getProperty(QnMediaResource::rtpTransportKey()),
        nx::vms::api::RtpTransportType::automatic);
}

CameraMediaStreams QnVirtualCameraResource::mediaStreams() const
{
    const QString& mediaStreamsStr = getProperty(ResourcePropertyKey::kMediaStreams);
    CameraMediaStreams supportedMediaStreams = QJson::deserialized<CameraMediaStreams>(mediaStreamsStr.toLatin1());
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
    using nx::vms::common::core::resource::SensorDescription;

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

    // Following logic only applies to Entropix cameras that have video from two sensors
    // in one video stream.
    const auto& sensor = combinedSensorsDescription().mainSensor();
    if (!sensor.isValid())
        return QnAspectRatio(size);

    const auto& sensorSize = sensor.geometry.size();

    const QSize realSensorSize = QSize(
        static_cast<int>(size.width() * sensorSize.width()),
        static_cast<int>(size.height() * sensorSize.height()));

    return QnAspectRatio(realSensorSize);
}

QnAspectRatio QnVirtualCameraResource::aspectRatioRotated() const
{
    return QnAspectRatio::isRotated90(defaultRotation())
        ? aspectRatio().inverted()
        : aspectRatio();
}

static bool isParamsCompatible(const CameraMediaStreamInfo& newParams, const CameraMediaStreamInfo& oldParams)
{
    if (newParams.codec != oldParams.codec)
        return false;
    bool streamParamsMatched = newParams.customStreamParams == oldParams.customStreamParams ||
         (newParams.customStreamParams.empty() && !oldParams.customStreamParams.empty());
    bool resolutionMatched = newParams.resolution == oldParams.resolution ||
         ((newParams.resolution == CameraMediaStreamInfo::anyResolution) && (oldParams.resolution != CameraMediaStreamInfo::anyResolution));
    return streamParamsMatched && resolutionMatched;
}

static bool transcodingAvailable()
{
    return !nx::utils::AppInfo::isArm() && !nx::utils::AppInfo::isEdgeServer();
}

bool QnVirtualCameraResource::saveMediaStreamInfoIfNeeded( const CameraMediaStreamInfo& mediaStreamInfo )
{
    //saving hasDualStreaming flag before locking mutex
    const auto hasDualStreamingLocal = hasDualStreaming();

    //TODO #ak remove m_mediaStreamsMutex lock, use resource mutex
    QnMutexLocker lk( &m_mediaStreamsMutex );

    //get saved stream info with index encoderIndex
    const QString& mediaStreamsStr = getProperty(ResourcePropertyKey::kMediaStreams);
    CameraMediaStreams supportedMediaStreams = QJson::deserialized<CameraMediaStreams>( mediaStreamsStr.toLatin1() );

    const bool isTranscodingAllowedByCurrentMediaStreamsParam = std::find_if(
        supportedMediaStreams.streams.begin(),
        supportedMediaStreams.streams.end(),
        []( const CameraMediaStreamInfo& mediaInfo ) {
            return mediaInfo.transcodingRequired;
        } ) != supportedMediaStreams.streams.end();

    bool needUpdateStreamInfo = true;
    if (isTranscodingAllowedByCurrentMediaStreamsParam == transcodingAvailable())
    {
        //checking if stream info has been changed
        for( auto it = supportedMediaStreams.streams.begin();
            it != supportedMediaStreams.streams.end();
            ++it )
        {
            if( it->encoderIndex == mediaStreamInfo.encoderIndex )
            {
                if( *it == mediaStreamInfo)
                    needUpdateStreamInfo = false;
                //if new media stream info does not contain resolution, preferring existing one
                if (isParamsCompatible(mediaStreamInfo, *it))
                    needUpdateStreamInfo = false;   //stream info has not been changed
                break;
            }
        }
    }
    //else
    //    we have to update information about transcoding availability anyway

    bool modified = false;
    if (needUpdateStreamInfo)
    {
        //removing stream with same encoder index as mediaStreamInfo
        QString previouslySavedResolution;
        supportedMediaStreams.streams.erase(
            std::remove_if(
                supportedMediaStreams.streams.begin(),
                supportedMediaStreams.streams.end(),
                [&mediaStreamInfo, &previouslySavedResolution]( CameraMediaStreamInfo& mediaInfo ) {
                    if( mediaInfo.encoderIndex == mediaStreamInfo.encoderIndex )
                    {
                        previouslySavedResolution = std::move(mediaInfo.resolution);
                        return true;
                    }
                    return false;
                } ),
            supportedMediaStreams.streams.end() );

        CameraMediaStreamInfo newMediaStreamInfo = mediaStreamInfo; //have to copy it anyway to save to supportedMediaStreams.streams
        if( !previouslySavedResolution.isEmpty() &&
            newMediaStreamInfo.resolution == CameraMediaStreamInfo::anyResolution )
        {
            newMediaStreamInfo.resolution = std::move(previouslySavedResolution);
        }
        //saving new stream info
        supportedMediaStreams.streams.push_back(std::move(newMediaStreamInfo));

        modified = true;
    }

    if (!hasDualStreamingLocal)
    {
        //checking whether second stream has disappeared
        auto secondStreamIter = std::find_if(
            supportedMediaStreams.streams.begin(),
            supportedMediaStreams.streams.end(),
            [](const CameraMediaStreamInfo& mediaInfo)
            {
                return mediaInfo.getEncoderIndex() == StreamIndex::secondary;
            });
        if (secondStreamIter != supportedMediaStreams.streams.end())
        {
            supportedMediaStreams.streams.erase(secondStreamIter);
            modified = true;
        }
    }

    if (!modified)
        return false;

    //removing non-native streams (they will be re-generated)
    supportedMediaStreams.streams.erase(
        std::remove_if(
            supportedMediaStreams.streams.begin(),
            supportedMediaStreams.streams.end(),
            []( const CameraMediaStreamInfo& mediaStreamInfo ) -> bool {
                return mediaStreamInfo.transcodingRequired;
            } ),
        supportedMediaStreams.streams.end() );

    saveResolutionList( supportedMediaStreams );

    return true;
}

bool QnVirtualCameraResource::saveMediaStreamInfoIfNeeded( const CameraMediaStreams& streams )
{
    bool rez = false;
    for (const auto& streamInfo: streams.streams)
        rez |= saveMediaStreamInfoIfNeeded(streamInfo);
    return rez;
}

bool QnVirtualCameraResource::saveBitrateIfNeeded( const CameraBitrateInfo& bitrateInfo )
{
    NX_ASSERT(bitrateInfo.resolution.indexOf('x') > 0, "Resolution: %1", bitrateInfo.resolution);

    auto bitrateInfos = QJson::deserialized<CameraBitrates>(
        getProperty(ResourcePropertyKey::kBitrateInfos).toLatin1() );

    auto it = std::find_if(bitrateInfos.streams.begin(), bitrateInfos.streams.end(),
                           [&](const CameraBitrateInfo& info)
                           { return info.encoderIndex == bitrateInfo.encoderIndex; });

    if (it != bitrateInfos.streams.end())
    {
        const auto time = QDateTime::fromString(bitrateInfo.timestamp, Qt::ISODate);
        const auto lastTime = QDateTime::fromString(it->timestamp, Qt::ISODate);

        // Generally update should not happen more often than once per
        // UPDATE_BITRATE_TIMEOUT_DAYS
        if (lastTime.isValid() && lastTime < time &&
            lastTime.addDays(UPDATE_BITRATE_TIMEOUT_DAYS) > time)
        {
            // If camera got configured we shell update anyway
            bool isGotConfigured = bitrateInfo.isConfigured &&
                it->isConfigured != bitrateInfo.isConfigured;

            if (!isGotConfigured)
                return false;
        }

        // override old data
        *it = bitrateInfo;
    }
    else
    {
        // add new record
        bitrateInfos.streams.push_back(std::move(bitrateInfo));
    }

    setProperty(ResourcePropertyKey::kBitrateInfos,
                QString::fromUtf8(QJson::serialized(bitrateInfos)));

    return true;
}

void QnVirtualCameraResource::emitPropertyChanged(const QString& key)
{
    if (key == ResourcePropertyKey::kPtzCapabilities)
        emit ptzCapabilitiesChanged(::toSharedPointer(this));
    else if (key == ResourcePropertyKey::kIoConfigCapability)
        emit isIOModuleChanged(::toSharedPointer(this));
    base_type::emitPropertyChanged(key);
}

void QnVirtualCameraResource::saveResolutionList( const CameraMediaStreams& supportedNativeStreams )
{
    static const char* RTSP_TRANSPORT_NAME = "rtsp";
    static const char* HLS_TRANSPORT_NAME = "hls";
    static const char* MJPEG_TRANSPORT_NAME = "mjpeg";

    CameraMediaStreams fullStreamList( supportedNativeStreams );
    for( std::vector<CameraMediaStreamInfo>::iterator
        it = fullStreamList.streams.begin();
        it != fullStreamList.streams.end();
         )
    {
        if( it->resolution.isEmpty() || it->resolution == lit("0x0") )
        {
            it = fullStreamList.streams.erase( it );
            continue;
        }
        it->transports.clear();

        switch( it->codec )
        {
            case AV_CODEC_ID_H264:
                it->transports.push_back( QLatin1String(RTSP_TRANSPORT_NAME) );
                it->transports.push_back( QLatin1String(HLS_TRANSPORT_NAME) );
                break;
            case AV_CODEC_ID_H265:
            case AV_CODEC_ID_MPEG4:
                it->transports.push_back( QLatin1String(RTSP_TRANSPORT_NAME) );
                break;
            case AV_CODEC_ID_MJPEG:
                it->transports.push_back( QLatin1String(MJPEG_TRANSPORT_NAME) );
                break;
            default:
                break;
        }

        ++it;
    }

    if (transcodingAvailable())
    {
        static const char* WEBM_TRANSPORT_NAME = "webm";

        CameraMediaStreamInfo transcodedStream;
        // Any resolution is supported.
        transcodedStream.transports.push_back(QLatin1String(RTSP_TRANSPORT_NAME));
        transcodedStream.transports.push_back(QLatin1String(MJPEG_TRANSPORT_NAME));
        transcodedStream.transports.push_back(QLatin1String(WEBM_TRANSPORT_NAME));
        transcodedStream.transcodingRequired = true;
        fullStreamList.streams.push_back(transcodedStream);
    }

    //saving fullStreamList;
    QByteArray serializedStreams = QJson::serialized( fullStreamList );
    NX_VERBOSE(this, "Set media stream information for camera %1, data: %2", getUrl(), serializedStreams);
    setProperty(ResourcePropertyKey::kMediaStreams, QString::fromUtf8(serializedStreams));
}

QnAdvancedStreamParams QnVirtualCameraResource::advancedLiveStreamParams() const
{
    NX_ASSERT(false, lm("This method should not be called on client side."));
    return QnAdvancedStreamParams();
}

QSet<QnUuid> QnVirtualCameraResource::enabledAnalyticsEngines() const
{
    return enabledAnalyticsEngineResources().ids().toSet();
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
    setProperty(
        kUserEnabledAnalyticsEnginesProperty,
        QString::fromUtf8(QJson::serialized(engines)));
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

std::map<QnUuid, std::set<QString>> QnVirtualCameraResource::supportedEventTypes() const
{
    return filterByActiveEngines(m_cachedSupportedEventTypes.get(), enabledAnalyticsEngines());
}

std::map<QnUuid, std::set<QString>> QnVirtualCameraResource::supportedObjectTypes() const
{
    return filterByActiveEngines(m_cachedSupportedObjectTypes.get(), enabledAnalyticsEngines());
}

QSet<QnUuid> QnVirtualCameraResource::calculateUserEnabledAnalyticsEngines() const
{
    return QJson::deserialized<QSet<QnUuid>>(
        getProperty(kUserEnabledAnalyticsEnginesProperty).toUtf8());
}

QSet<QnUuid> QnVirtualCameraResource::calculateCompatibleAnalyticsEngines() const
{
    return QJson::deserialized<QSet<QnUuid>>(
        getProperty(kCompatibleAnalyticsEnginesProperty).toUtf8());
}

std::map<QnUuid, std::set<QString>> QnVirtualCameraResource::calculateSupportedEntities(
     ManifestItemIdsFetcher fetcher) const
{
    std::map<QnUuid, std::set<QString>> result;
    auto deviceAgentManifests = m_cachedDeviceAgentManifests.get();
    for (const auto& [engineId, deviceAgentManifest]: deviceAgentManifests)
        result[engineId] = fetcher(deviceAgentManifest);

    return result;
}

std::map<QnUuid, std::set<QString>> QnVirtualCameraResource::calculateSupportedEventTypes() const
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

            return result;
        });
}

std::map<QnUuid, std::set<QString>> QnVirtualCameraResource::calculateSupportedObjectTypes() const
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

            return result;
        });
}

QnVirtualCameraResource::DeviceAgentManifestMap
    QnVirtualCameraResource::fetchDeviceAgentManifests() const
{
    return QJson::deserialized<DeviceAgentManifestMap>(
        getProperty(kDeviceAgentManifestsProperty).toUtf8());
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
        QString::fromUtf8(QJson::serialized(manifests)));
}

nx::vms::api::StreamIndex QnVirtualCameraResource::analyzedStreamIndex(QnUuid engineId) const
{
    using nx::vms::api::analytics::DeviceAgentManifest;

    const std::optional<DeviceAgentManifest> deviceAgentManifest =
        this->deviceAgentManifest(engineId);

    const bool userIsAllowedToSelectStream = !deviceAgentManifest
        || !deviceAgentManifest->capabilities.testFlag(
            DeviceAgentManifest::Capability::disableStreamSelection);

    if (userIsAllowedToSelectStream)
    {
        const QString serializedProperty = getProperty(kAnalyzedStreamIndexes);
        if (serializedProperty.isEmpty())
            return kDefaultAnalyzedStreamIndex;

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

        if (analyzedStreamIndexMap.contains(engineId))
            return analyzedStreamIndexMap[engineId];
    }

    const QnResourcePool* const resourcePool = this->resourcePool();
    if (!NX_ASSERT(resourcePool))
        return kDefaultAnalyzedStreamIndex;

    const auto engineResource = resourcePool->getResourceById(engineId)
        .dynamicCast<nx::vms::common::AnalyticsEngineResource>();

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
    const auto supportsUserDefinedStreams = hasCameraCapabilities(Qn::CustomMediaUrlCapability);
    if (supportsUserDefinedStreams)
    {
        const auto primaryStream = sourceUrl(Qn::CR_LiveVideo);
        const auto secondaryStream = sourceUrl(Qn::CR_SecondaryLiveVideo);
        if (!primaryStream.isEmpty() && !secondaryStream.isEmpty())
            return true;
    }

    return base_type::hasDualStreamingInternal();
}

Qn::RecordingState QnVirtualCameraResource::recordingState() const
{
    if (!isLicenseUsed())
        return Qn::RecordingState::Off;
    if (getStatus() == Qn::Recording)
        return Qn::RecordingState::On;
    return Qn::RecordingState::Scheduled;
}
