#include "camera_resource.h"

#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>

#include <nx/vms/api/data/camera_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_camera_manager.h>

#include <common/common_module.h>
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

} //anonymous namespace

const QString QnVirtualCameraResource::kEnabledAnalyticsEnginesProperty("enabledAnalyticsEngines");
const QString QnVirtualCameraResource::kDeviceAgentsSettingsValuesProperty(
    "deviceAgentsSettingsValuesProperty");

const QString QnVirtualCameraResource::kDeviceAgentManifestsProperty("deviceAgentManifests");

const QString QnVirtualCameraResource::kSupportedAnalyticsEventsProperty(
    "supportedAnalyticsEvents");
const QString QnVirtualCameraResource::kSupportedAnalyticsObjectsProperty(
    "supportedAnalyticsObjects");

QnVirtualCameraResource::QnVirtualCameraResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_issueCounter(0),
    m_lastIssueTimer(),
    m_cachedAnalyticsEngines([this] { return calculateEnabledAnalyticsEngines(); }, &m_cacheMutex)
{
    connect(this, &QnResource::propertyChanged,
        this, [&]{ m_cachedAnalyticsEngines.reset(); });
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

    auto cachedUrl = m_cachedStreamUrls.find(role);
    bool cachedUrlExists = cachedUrl != m_cachedStreamUrls.end();

    if (cachedUrlExists && cachedUrl->second == url)
        return;

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

int QnVirtualCameraResource::issuesTimeoutMs() {
    return ISSUE_KEEP_TIMEOUT_MS;
}

CameraMediaStreams QnVirtualCameraResource::mediaStreams() const
{
    const QString& mediaStreamsStr = getProperty(ResourcePropertyKey::kMediaStreams);
    CameraMediaStreams supportedMediaStreams = QJson::deserialized<CameraMediaStreams>(mediaStreamsStr.toLatin1());
    return supportedMediaStreams;
}

CameraMediaStreamInfo QnVirtualCameraResource::streamInfo(Qn::StreamIndex index) const
{
    const auto streams = mediaStreams().streams;
    auto stream = std::find_if(streams.cbegin(), streams.cend(),
        [index](const CameraMediaStreamInfo& stream)
        {
            return (Qn::StreamIndex) stream.encoderIndex == index;
        });
    if (stream != streams.cend())
        return *stream;

    return CameraMediaStreamInfo();
}

QnAspectRatio QnVirtualCameraResource::aspectRatio() const
{
    using nx::vms::common::core::resource::SensorDescription;

    const auto customAr = customAspectRatio();

    if (customAr.isValid())
        return customAr;

    // The rest of the code deals with auto aspect ratio.
    // Aspect ration should be forced to AS of the first stream. Note: primary stream AR could be
    // changed on the fly and a camera may not have a primary stream. In this case natural
    // secondary stream AS should be used.
    const auto stream = streamInfo(Qn::StreamIndex::primary);
    QSize size = stream.getResolution();
    if (size.isEmpty())
    {
        // Trying to use size from secondary stream
        const auto secondary = streamInfo(Qn::StreamIndex::secondary);
        size = secondary.getResolution();
    }

    if (size.isEmpty())
        return QnAspectRatio();

    const auto& sensor = combinedSensorsDescription().mainSensor();
    if (!sensor.isValid())
        return QnAspectRatio(size);

    const auto& sensorSize = sensor.geometry.size();

    const QSize realSensorSize = QSize(
        static_cast<int>(size.width() * sensorSize.width()),
        static_cast<int>(size.height() * sensorSize.height()));

    return QnAspectRatio(realSensorSize);
}

QnVirtualCameraResource::MotionStreamIndex QnVirtualCameraResource::motionStreamIndex() const
{
    const auto forcedMotionStreamStr = getProperty(QnMediaResource::motionStreamKey()).toLower();
    if (forcedMotionStreamStr.isEmpty())
    {
        if (!hasDualStreaming() && (getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability))
            return {Qn::StreamIndex::primary, /*isForced*/ false};

        return {Qn::StreamIndex::secondary, /*isForced*/ false};
    }

    const auto forcedMotionStream =
        QnLexical::deserialized<Qn::StreamIndex>(forcedMotionStreamStr, Qn::StreamIndex::undefined);
    NX_ASSERT(forcedMotionStream != Qn::StreamIndex::undefined);
    return {forcedMotionStream, /*isForced*/ true};
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

#if !defined(EDGE_SERVER) && !defined(__arm__)
#define TRANSCODING_AVAILABLE
static const bool transcodingAvailable = true;
#else
static const bool transcodingAvailable = false;
#endif

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
    if( isTranscodingAllowedByCurrentMediaStreamsParam == transcodingAvailable )
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
            [](const CameraMediaStreamInfo& mediaInfo) {
                return (Qn::StreamIndex) mediaInfo.encoderIndex == Qn::StreamIndex::secondary;
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

         // TODO: #GDM change to bool transcodingAvailable variable check
#ifdef TRANSCODING_AVAILABLE
    static const char* WEBM_TRANSPORT_NAME = "webm";

    CameraMediaStreamInfo transcodedStream;
    //any resolution is supported
    transcodedStream.transports.push_back( QLatin1String(RTSP_TRANSPORT_NAME) );
    transcodedStream.transports.push_back( QLatin1String(MJPEG_TRANSPORT_NAME) );
    transcodedStream.transports.push_back( QLatin1String(WEBM_TRANSPORT_NAME) );
    transcodedStream.transcodingRequired = true;
    fullStreamList.streams.push_back( transcodedStream );
#endif

    //saving fullStreamList;
    QByteArray serializedStreams = QJson::serialized( fullStreamList );
    setProperty(ResourcePropertyKey::kMediaStreams, QString::fromUtf8(serializedStreams));
}

QnAdvancedStreamParams QnVirtualCameraResource::advancedLiveStreamParams() const
{
    NX_ASSERT(false, lm("This method should not be called on client side."));
    return QnAdvancedStreamParams();
}

const nx::vms::common::AnalyticsEngineResourceList
    QnVirtualCameraResource::enabledAnalyticsEngineResources() const
{
    auto enabledEngines = enabledAnalyticsEngines();
    return resourcePool()->getResourcesByIds<nx::vms::common::AnalyticsEngineResource>(enabledEngines);
}

QSet<QnUuid> QnVirtualCameraResource::enabledAnalyticsEngines() const
{
    return m_cachedAnalyticsEngines.get();
}

void QnVirtualCameraResource::setEnabledAnalyticsEngines(const QSet<QnUuid>& engines)
{
    setProperty(kEnabledAnalyticsEnginesProperty, QString::fromUtf8(QJson::serialized(engines)));
}

QHash<QnUuid, QVariantMap> QnVirtualCameraResource::deviceAgentSettingsValues() const
{
    const auto values = QJson::deserialized<QHash<QnUuid, QJsonObject>>(
        getProperty(kDeviceAgentsSettingsValuesProperty).toUtf8());

    QHash<QnUuid, QVariantMap> result;
    for (auto it = values.begin(); it != values.end(); ++it)
        result.insert(it.key(), it.value().toVariantMap());
    return result;
}

void QnVirtualCameraResource::setDeviceAgentSettingsValues(
    const QHash<QnUuid, QVariantMap>& settingsValues)
{
    QHash<QnUuid, QJsonObject> result;
    for (auto it = settingsValues.begin(); it != settingsValues.end(); ++it)
        result.insert(it.key(), QJsonObject::fromVariantMap(it.value()));

    setProperty(kDeviceAgentsSettingsValuesProperty, QString::fromUtf8(QJson::serialized(result)));
    saveProperties();
}

QVariantMap QnVirtualCameraResource::deviceAgentSettingsValues(const QnUuid& engineId) const
{
    return deviceAgentSettingsValues()[engineId];
}

void QnVirtualCameraResource::setDeviceAgentSettingsValues(
    const QnUuid& engineId, const QVariantMap& settingsValues)
{
    auto settingsValuesByEngineId = deviceAgentSettingsValues();
    settingsValuesByEngineId[engineId] = settingsValues;
    setDeviceAgentSettingsValues(settingsValuesByEngineId);
}

std::optional<nx::vms::api::analytics::DeviceAgentManifest>
    QnVirtualCameraResource::deviceAgentManifest(const QnUuid& engineId)
{
    using namespace nx::vms::api::analytics;
    auto manifestsStr = getProperty(kDeviceAgentManifestsProperty);

    bool success = false;
    auto manifests = QJson::deserialized<std::map<QnUuid, DeviceAgentManifest>>(
        manifestsStr.toUtf8(), std::map<QnUuid, DeviceAgentManifest>(), &success);

    if (!success)
        return std::nullopt;

    auto itr = manifests.find(engineId);
    if (itr == manifests.cend())
        return std::nullopt;

    return itr->second;
}

void QnVirtualCameraResource::setDeviceAgentManifest(
    const QnUuid& engineId,
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    using namespace nx::vms::api::analytics;
    auto manifestsStr = getProperty(kDeviceAgentManifestsProperty);

    auto manifests = QJson::deserialized<std::map<QnUuid, DeviceAgentManifest>>(
        manifestsStr.toUtf8(), std::map<QnUuid, DeviceAgentManifest>());

    manifests[engineId] = manifest;
    setProperty(
        kDeviceAgentManifestsProperty,
        QString::fromUtf8(QJson::serialized(manifests)));
    saveProperties();
}

void QnVirtualCameraResource::setSupportedAnalyticsEventTypeIds(
    QnUuid engineId, QSet<QString> supportedEvents)
{
    setSupportedAnalyticsItemTypeIds(
        kSupportedAnalyticsEventsProperty,
        std::move(engineId),
        std::move(supportedEvents));
}

QMap<QnUuid, QSet<QString>> QnVirtualCameraResource::supportedAnalyticsEventTypeIds() const
{
    return supportedAnalyticsItemTypeIds(kSupportedAnalyticsEventsProperty);
}

QSet<QString> QnVirtualCameraResource::supportedAnalyticsEventTypeIds(const QnUuid& engineId) const
{
    return supportedAnalyticsItemTypeIds(kSupportedAnalyticsEventsProperty, engineId);
}

void QnVirtualCameraResource::setSupportedAnalyticsObjectTypeIds(
    QnUuid engineId, QSet<QString> supportedObjects)
{
    setSupportedAnalyticsItemTypeIds(
        kSupportedAnalyticsObjectsProperty,
        std::move(engineId),
        std::move(supportedObjects));
}

QMap<QnUuid, QSet<QString>> QnVirtualCameraResource::supportedAnalyticsObjectTypeIds() const
{
    return supportedAnalyticsItemTypeIds(kSupportedAnalyticsObjectsProperty);
}

QSet<QString> QnVirtualCameraResource::supportedAnalyticsObjectTypeIds(const QnUuid& engineId) const
{
    return supportedAnalyticsItemTypeIds(kSupportedAnalyticsObjectsProperty, engineId);
}

void QnVirtualCameraResource::setSupportedAnalyticsItemTypeIds(
    const QString& propertyName,
    QnUuid engineId,
    QSet<QString> supportedItems)
{
    using SupportedItemMap = QMap<QnUuid, QSet<QString>>;
    auto serialized = getProperty(propertyName);
    auto supportedItemMap = QJson::deserialized(serialized.toUtf8(), SupportedItemMap());

    supportedItemMap.insert(std::move(engineId), std::move(supportedItems));

    serialized = QString::fromUtf8(QJson::serialized(supportedItemMap));
    setProperty(propertyName, serialized);
    saveProperties();
}

QMap<QnUuid, QSet<QString>> QnVirtualCameraResource::supportedAnalyticsItemTypeIds(
    const QString& propertyName) const
{
    using SupportedItemMap = QMap<QnUuid, QSet<QString>>;
    const auto serialized = getProperty(propertyName);
    return QJson::deserialized(serialized.toUtf8(), SupportedItemMap());
}
QSet<QString> QnVirtualCameraResource::supportedAnalyticsItemTypeIds(
    const QString& propertyName,
    const QnUuid& engineId) const
{
    using SupportedItemMap = QMap<QnUuid, QSet<QString>>;
    const auto serialized = getProperty(propertyName);
    const auto supportedItemMap = QJson::deserialized(serialized.toUtf8(), SupportedItemMap());

    return supportedItemMap.value(engineId);
}

QSet<QnUuid> QnVirtualCameraResource::calculateEnabledAnalyticsEngines()
{
    return QJson::deserialized<QSet<QnUuid>>(
        getProperty(kEnabledAnalyticsEnginesProperty).toUtf8());
}