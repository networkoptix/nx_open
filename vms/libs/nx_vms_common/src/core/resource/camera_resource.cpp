// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_resource.h"

#include <QtCore/QUrlQuery>

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

#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/string_conversion.h>
#include <utils/common/util.h>
#include <utils/crypt/symmetrical.h>
#include <utils/math/math.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/qset.h>

#include <nx/build_info.h>
#include <nx/kit/utils.h>

namespace {

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

QnVirtualCameraResource::QnVirtualCameraResource(QnCommonModule* commonModule):
    base_type(commonModule),
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

    NX_VERBOSE(this, nx::format("Update %1 stream %2 URL: %3").args(getPhysicalId(), role, url));
    if (!storeUrlForRole(role))
        return;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
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

    NX_DEBUG(this, nx::format("Save %1 stream %2 URL: %3").args(getPhysicalId(), role, url));
    if (updateProperty(ResourcePropertyKey::kStreamUrls, urlUpdater))
    {
        //TODO: #rvasilenko Setter and saving must be split.
        if (save)
            saveProperties();
    }
}

int QnVirtualCameraResource::updateAsync()
{
    nx::vms::api::CameraData apiCamera;
    ec2::fromResourceToApi(toSharedPointer(this), apiCamera);

    const QnResourcePool* const resourcePool = this->resourcePool();
    if (resourcePool && resourcePool->getResourceById<QnVirtualCameraResource>(apiCamera.id))
    {
        ec2::AbstractECConnectionPtr conn = commonModule()->ec2Connection();
        return conn->getCameraManager(Qn::kSystemAccess)->addCamera(apiCamera,
            [](int /*requestId*/, ec2::ErrorCode) {});
    }

    return (int) ec2::ErrorCode::ok;
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
    setProperty(kVirtualCameraIgnoreTimeZone, value ? true : QVariant());
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
    return !nx::build_info::isArm() && !nx::build_info::isEdgeServer();
}

bool QnVirtualCameraResource::saveMediaStreamInfoIfNeeded( const CameraMediaStreamInfo& mediaStreamInfo )
{
    //saving hasDualStreaming flag before locking mutex
    const auto hasDualStreamingLocal = hasDualStreaming();

    //TODO #akolesnikov remove m_mediaStreamsMutex lock, use resource mutex
    NX_MUTEX_LOCKER lk( &m_mediaStreamsMutex );

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
        if (lastTime.isValid()
            && lastTime.addDays(UPDATE_BITRATE_TIMEOUT_DAYS) < time)
        {
            // If camera got configured we shell update anyway
            bool isGotConfigured = bitrateInfo.isConfigured ||
                it->isConfigured != bitrateInfo.isConfigured;

            if (!isGotConfigured)
                return false;
        }
        else
        {
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
    NX_ASSERT(false, nx::format("This method should not be called on client side."));
    return QnAdvancedStreamParams();
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
    return QJson::deserialized<QSet<QnUuid>>(
        getProperty(kUserEnabledAnalyticsEnginesProperty).toUtf8());
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

Qn::RecordingState QnVirtualCameraResource::recordingState() const
{
    if (!isLicenseUsed())
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
