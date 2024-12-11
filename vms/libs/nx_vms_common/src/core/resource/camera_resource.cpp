// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_resource.h"

#include <QtCore/QUrlQuery>

#include <api/model/api_ioport_data.h>
#include <common/common_module.h>
#include <core/ptz/ptz_preset.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource/resource_data.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <licensing/license.h>
#include <nx/build_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/kit/utils.h>
#include <nx/media/stream_event.h>
#include <nx/network/rest/user_access_data.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/crypt/symmetrical.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/math/math.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/serialization/qt_geometry_reflect_json.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <recording/time_period_list.h>
#include <utils/camera/camera_bitrate_calculator.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

#include "media_server_resource.h"
#include "resource_data.h"
#include "resource_data_structures.h"
#include "resource_property_key.h"

#define SAFE(expr) {NX_MUTEX_LOCKER lock( &m_mutex ); expr;}

namespace {

using namespace nx::vms::common;

using StreamFpsSharingMethod = QnVirtualCameraResource::StreamFpsSharingMethod;

const QString kPowerRelayPortName =
    QString::fromStdString(nx::reflect::toString(nx::vms::api::ExtendedCameraOutput::powerRelay));

static const int kShareFpsDefaultReservedSecondStreamFps = 2;
static const int kSharePixelsDefaultReservedSecondStreamFps = 0;
static const StreamFpsSharingMethod kDefaultStreamFpsSharingMethod = StreamFpsSharingMethod::pixels;

static const QMap<StreamFpsSharingMethod, QString> kFpsSharingMethodToString{
    {StreamFpsSharingMethod::basic, "shareFps"},
    {StreamFpsSharingMethod::pixels, "sharePixels"},
    {StreamFpsSharingMethod::none, "noSharing"},
};

static const QString kRemoteArchiveMotionDetectionKey("remoteArchiveMotionDetection");

bool isStringEmpty(const QString& value)
{
    return value.isEmpty();
};

bool isDeviceTypeEmpty(nx::vms::api::DeviceType value)
{
    return value == nx::vms::api::DeviceType::unknown;
};

QString boolToPropertyStr(bool value)
{
    return value ? lit("1") : lit("0");
}

bool cameraExists(QnResourcePool* resourcePool, const nx::Uuid& uuid)
{
    return resourcePool && resourcePool->getResourceById<QnVirtualCameraResource>(uuid);
}

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

namespace nx::vms::api::analytics {
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceAgentManifest, (json), DeviceAgentManifest_Fields, (brief, true))

} // namespace nx::vms::api::analytics

using AnalyzedStreamIndexMap = QMap<nx::Uuid, nx::vms::api::StreamIndex>;
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
    m_manuallyAdded(false),
    m_cachedAudioRequired(
        [this] {
            if (isAudioEnabled())
                return true;

            if (auto context = systemContext())
            {
                return context->resourcePropertyDictionary()->hasProperty(
                    ResourcePropertyKey::kAudioInputDeviceId,
                    getId().toString());
            }

            return false;
        }),
    m_cachedRtspMetadataDisabled(
        [this]()
        {
            return resourceData().value(ResourceDataKey::kDisableRtspMetadataStream, false);
        }),
    m_cachedLicenseType([this] { return calculateLicenseType(); }),
    m_cachedHasDualStreaming(
        [this]()->bool
        {
            return hasDualStreamingInternal()
                && !isDualStreamingDisabled();
        }),
    m_cachedSupportedMotionTypes(
        std::bind(&QnVirtualCameraResource::calculateSupportedMotionTypes, this)),
    m_cachedCameraCapabilities(
        [this]()
        {
            return static_cast<nx::vms::api::DeviceCapabilities>(
                getProperty(ResourcePropertyKey::kCameraCapabilities).toInt());
        }),
    m_cachedIsDtsBased(
        [this]()->bool{ return getProperty(ResourcePropertyKey::kDts).toInt() > 0; }),
    m_motionType(std::bind(&QnVirtualCameraResource::calculateMotionType, this)),
    m_cachedIsIOModule(
        [this]() { return getProperty(ResourcePropertyKey::kIoConfigCapability).toInt() > 0; }),
    m_cachedCanConfigureRemoteRecording(
        [this]()
        {
            return getProperty(ResourcePropertyKey::kCanConfigureRemoteRecording).toInt() > 0;
        }),
    m_cachedCameraMediaCapabilities(
        [this]()
        {
            return QJson::deserialized<nx::vms::api::CameraMediaCapability>(
                getProperty(ResourcePropertyKey::kMediaCapabilities).toUtf8());
        }),
    m_cachedExplicitDeviceType(
        [this]()
        {
            return nx::reflect::fromString(
                getProperty(ResourcePropertyKey::kDeviceType).toStdString(),
                nx::vms::api::DeviceType::unknown);
        }),
    m_cachedMotionStreamIndex([this]{ return calculateMotionStreamIndex(); }),
    m_cachedIoPorts(
        [this]()
        {
            return std::get<0>(nx::reflect::json::deserialize<QnIOPortDataList>(
                nx::toBufferView(getProperty(ResourcePropertyKey::kIoSettings).toUtf8())));
        }),
    m_cachedHasVideo(
        [this]()
        {
            auto result = getProperty(ResourcePropertyKey::kNoVideoSupport);
            if (!result.isEmpty())
                return result.toInt() == 0;

            return !resourceData().value<bool>(ResourceDataKey::kNoVideoSupport);
        }),
    m_isIntercom(
        [this]()
        {
            const std::string ioSettings =
                getProperty(ResourcePropertyKey::kIoSettings).toStdString();
            const auto [portDescriptions, _] =
                nx::reflect::json::deserialize<QnIOPortDataList>(ioSettings);

            return std::any_of(
                portDescriptions.begin(),
                portDescriptions.end(),
                [](const auto& portData)
                {
                    return portData.outputName == intercomSpecificPortName();
                });
        }),
    m_cachedUserEnabledAnalyticsEngines(
        [this]() { return calculateUserEnabledAnalyticsEngines(); }),
    m_cachedCompatibleAnalyticsEngines(
        [this]() { return calculateCompatibleAnalyticsEngines(); }),
    m_cachedDeviceAgentManifests(
        [this]() { return deviceAgentManifests(); }),
    m_cachedSupportedEventTypes(
        [this]() { return calculateSupportedEventTypes(); }),
    m_cachedSupportedObjectTypes(
        [this]() { return calculateSupportedObjectTypes(); }),
    m_cachedMediaStreams(
        [this]()
        {
            const QString& mediaStreamsStr = getProperty(ResourcePropertyKey::kMediaStreams);
            CameraMediaStreams supportedMediaStreams =
                QJson::deserialized<CameraMediaStreams>(mediaStreamsStr.toLatin1());

            // TODO #lbusygin: just for vms_4.2 version, get rid of this code when the mobile client
            // stops supporting servers < 5.0.
            for (auto& str: supportedMediaStreams.streams)
            {
                if (str.codec == AV_CODEC_ID_INDEO3)
                    str.codec = AV_CODEC_ID_H264;
                if (str.codec == AV_CODEC_ID_FIC)
                    str.codec = AV_CODEC_ID_H265;
            }
            return supportedMediaStreams;
        }),
    m_cachedBackupMegapixels([this]() { return backupMegapixels(getActualBackupQualities()); }),
    m_primaryStreamConfiguration(
        [this]()
        {
            return QJson::deserialized<QnLiveStreamParams>(
                getProperty(ResourcePropertyKey::kPrimaryStreamConfiguration).toUtf8());
        }),
    m_secondaryStreamConfiguration(
        [this]()
        {
            return QJson::deserialized<QnLiveStreamParams>(
                getProperty(ResourcePropertyKey::kSecondaryStreamConfiguration).toUtf8());
        })
{
    NX_VERBOSE(this, "Creating");
    addFlags(Qn::live_cam);

    connect(this, &QnResource::resourceChanged, this, &QnVirtualCameraResource::resetCachedValues,
        Qt::DirectConnection);
    connect(this, &QnResource::propertyChanged, this, &QnVirtualCameraResource::resetCachedValues,
        Qt::DirectConnection);
    connect(
        this, &QnVirtualCameraResource::licenseTypeChanged,
        this, &QnVirtualCameraResource::resetCachedValues,
        Qt::DirectConnection);
    connect(
        this, &QnVirtualCameraResource::parentIdChanged,
        this, &QnVirtualCameraResource::resetCachedValues,
        Qt::DirectConnection);
    connect(
        this, &QnVirtualCameraResource::motionRegionChanged,
        this, &QnVirtualCameraResource::at_motionRegionChanged,
        Qt::DirectConnection);

    // TODO: #sivanov Override emitPropertyChanged instead.
    connect(this, &QnResource::propertyChanged,
        [this](const QnResourcePtr& /*resource*/, const QString& key, const QString& prevValue, const QString& newValue)
        {
          if (key == ResourcePropertyKey::kAudioOutputDeviceId)
          {
              updateAudioRequiredOnDevice(prevValue);
              updateAudioRequiredOnDevice(newValue);
          }
        }
    );

    connect(
        this, &QnVirtualCameraResource::audioEnabledChanged,
        this, [this]() { updateAudioRequired(); },
        Qt::DirectConnection);


    connect(this, &QnNetworkResource::propertyChanged,
        [this](const QnResourcePtr& /*resource*/, const QString& key)
        {
          if (key == ResourcePropertyKey::kCameraCapabilities)
          {
              emit capabilitiesChanged(toSharedPointer());
          }
          else if (key == ResourcePropertyKey::kForcedLicenseType)
          {
              emit licenseTypeChanged(toSharedPointer(this));
          }
          else if (key == ResourcePropertyKey::kAudioInputDeviceId)
          {
              emit audioInputDeviceIdChanged(toSharedPointer(this));
          }
          else if (key == ResourcePropertyKey::kTwoWayAudioEnabled)
          {
              emit twoWayAudioEnabledChanged(toSharedPointer(this));
          }
          else if (key == ResourcePropertyKey::kAudioOutputDeviceId)
          {
              emit audioOutputDeviceIdChanged(toSharedPointer(this));
          }
          else if (key == ResourcePropertyKey::kSupportedMotion)
          {
              m_cachedSupportedMotionTypes.update();
          }
          else if (key == QnMediaResource::kRotationKey)
          {
              emit rotationChanged();
          }
          else if (key == ResourcePropertyKey::kMediaCapabilities)
          {
              m_cachedCameraMediaCapabilities.reset();
              emit mediaCapabilitiesChanged(toSharedPointer(this));
          }
          else if (key == ResourcePropertyKey::kCameraHotspotsEnabled)
          {
              emit cameraHotspotsEnabledChanged(toSharedPointer(this));
          }
          else if (key == ResourcePropertyKey::kCameraHotspotsData)
          {
              emit cameraHotspotsChanged(toSharedPointer(this));
          }
        });

    QnMediaResource::initMediaResource();
}

bool QnVirtualCameraResource::MotionStreamIndex::operator==(const MotionStreamIndex& other) const
{
    return index == other.index && isForced == other.isForced;
}

bool QnVirtualCameraResource::MotionStreamIndex::operator!=(const MotionStreamIndex& other) const
{
    return !(*this == other);
}

nx::Uuid QnVirtualCameraResource::makeCameraIdFromPhysicalId(const QString& physicalId)
{
    // ATTENTION: This logic is similar to the one in nx::vms::api::CameraData::fillId().
    if (physicalId.isEmpty())
        return nx::Uuid();
    return guidFromArbitraryData(physicalId);
}

QString QnVirtualCameraResource::intercomSpecificPortName()
{
    return kPowerRelayPortName;
}

void QnVirtualCameraResource::setSystemContext(nx::vms::common::SystemContext* systemContext)
{
    if (auto context = this->systemContext())
    {
        context->resourceDataPool()->disconnect(this);
        context->saasServiceManager()->disconnect(this);
    }

    base_type::setSystemContext(systemContext);

    if (auto context = this->systemContext())
    {
        connect(context->resourceDataPool(), &QnResourceDataPool::changed, this,
            &QnVirtualCameraResource::resetCachedValues, Qt::DirectConnection);
        connect(
            context->saasServiceManager(),
            &nx::vms::common::saas::ServiceManager::dataChanged,
            this,
            &QnVirtualCameraResource::resetCachedValues,
            Qt::DirectConnection);
        resetCachedValues();
    }
}

nx::vms::api::DeviceType QnVirtualCameraResource::calculateDeviceType(
    bool isDtsBased,
    bool isIoModule,
    bool isAnalogEncoder,
    bool isAnalog,
    bool isGroupIdEmpty,
    bool hasVideo,
    bool hasAudioCapability)
{
    using nx::vms::api::DeviceType;

    if (isDtsBased)
        return DeviceType::nvr;

    if (isIoModule)
        return DeviceType::ioModule;

    if (isAnalogEncoder || isAnalog)
        return DeviceType::encoder;

    if (!isGroupIdEmpty)
        return DeviceType::multisensorCamera;

    if (hasVideo)
        return DeviceType::camera;

    if (hasAudioCapability)
        return DeviceType::hornSpeaker;

    return DeviceType::unknown;
}

nx::vms::api::DeviceType QnVirtualCameraResource::deviceType() const
{
    // TODO: remove all setters (setIoModule, setIsDtsBased e.t.c) and keep setDeviceType only
    if (enforcedDeviceType() != nx::vms::api::DeviceType::unknown)
        return enforcedDeviceType();

    // TODO: #vbreus Seems that all properties should be fetched under the single lock.
    return calculateDeviceType(
        isDtsBased(),
        isIOModule(),
        isAnalogEncoder(),
        isAnalog(),
        getGroupId().isEmpty(),
        hasVideo(),
        hasTwoWayAudio());
}

QnMediaServerResourcePtr QnVirtualCameraResource::getParentServer() const {
    return getParentResource().dynamicCast<QnMediaServerResource>();
}

nx::Uuid QnVirtualCameraResource::getTypeId() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_typeId;
}

void QnVirtualCameraResource::setTypeId(const nx::Uuid& id)
{
    if (!NX_ASSERT(!id.isNull()))
        return;

    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    m_typeId = id;
}

QString QnVirtualCameraResource::getProperty(const QString& key) const
{
    if (auto value = QnResource::getProperty(key); !value.isEmpty())
        return value;

    // Find the default value in the Resource type.
    if (QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId()))
        return resType->defaultValue(key);

    return {};
}

bool QnVirtualCameraResource::isGroupPlayOnly() const
{
    // This parameter can never be overridden. Just read the Resource type info.
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(getTypeId());
    return resourceType && resourceType->hasParam(ResourcePropertyKey::kGroupPlayParamName);
}

bool QnVirtualCameraResource::needsToChangeDefaultPassword() const
{
    return isDefaultAuth()
           && hasCameraCapabilities(nx::vms::api::DeviceCapability::setUserPassword);
}

void QnVirtualCameraResource::updateAudioRequiredOnDevice(const QString& deviceId)
{
    auto prevDevice = nx::Uuid(deviceId);
    if (!prevDevice.isNull())
    {
        auto resource = systemContext()->resourcePool()->getResourceById<QnVirtualCameraResource>(
            prevDevice);
        if (resource)
            resource->updateAudioRequired();
    }
}

void QnVirtualCameraResource::updateAudioRequired()
{
    auto prevValue = m_cachedAudioRequired.get();
    if (prevValue != m_cachedAudioRequired.updated())
        emit audioRequiredChanged(::toSharedPointer(this));
}

void QnVirtualCameraResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    base_type::updateInternal(source, notifiers);

    QnVirtualCameraResourcePtr other_casted = qSharedPointerDynamicCast<QnVirtualCameraResource>(source);
    if (NX_ASSERT(other_casted))
    {
        if (other_casted->m_groupId != m_groupId)
        {
            m_groupId = other_casted->m_groupId;
            notifiers << [r = toSharedPointer(this), previousGroupId = other_casted->m_groupId]
            {
              emit r->groupIdChanged(r, previousGroupId);
            };
        }

        if (other_casted->m_groupName != m_groupName)
        {
            m_groupName = other_casted->m_groupName;
            notifiers << [r = toSharedPointer(this)]{ emit r->groupNameChanged(r); };
        }

        if (other_casted->m_statusFlags != m_statusFlags)
        {
            m_statusFlags = other_casted->m_statusFlags;
            notifiers << [r = toSharedPointer(this)]{ emit r->statusFlagsChanged(r); };
        }

        m_manuallyAdded = other_casted->m_manuallyAdded;
        m_model = other_casted->m_model;
        m_vendor = other_casted->m_vendor;
    }
}

int QnVirtualCameraResource::getMaxFps(nx::vms::api::StreamIndex streamIndex) const
{
    const auto capabilities = cameraMediaCapability();
    if (auto f = capabilities.streamCapabilities.find(streamIndex);
        f != capabilities.streamCapabilities.cend() && f->second.maxFps > 0)
    {
        return f->second.maxFps;
    }

    // Compatibility with version < 3.1.2
    QString value = getProperty(ResourcePropertyKey::kMaxFps);
    return value.isNull() ? kDefaultMaxFps : value.toInt();
}

void QnVirtualCameraResource::setMaxFps(int fps, nx::vms::api::StreamIndex streamIndex)
{
    nx::vms::api::CameraMediaCapability capability = cameraMediaCapability();
    capability.streamCapabilities[streamIndex].maxFps = fps;
    setCameraMediaCapability(capability);
}

int QnVirtualCameraResource::reservedSecondStreamFps() const
{
    QString value = getProperty("reservedSecondStreamFps");

    if (!value.isNull())
    {
        bool ok = false;
        int reservedSecondStreamFps = value.toInt(&ok);

        if (ok)
            return reservedSecondStreamFps;

        NX_WARNING(this, "Wrong reserved second stream fps value for camera %1", this);
    }

    const StreamFpsSharingMethod sharingMethod = streamFpsSharingMethod();
    switch (sharingMethod)
    {
        case StreamFpsSharingMethod::basic:
            return kShareFpsDefaultReservedSecondStreamFps;

        case StreamFpsSharingMethod::pixels:
            return kSharePixelsDefaultReservedSecondStreamFps;

        default:
            return 0;
    }
}

bool QnVirtualCameraResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return m_cachedHasVideo.get();
}

Qn::LicenseType QnVirtualCameraResource::calculateLicenseType() const
{
    using namespace nx::vms::api;
    const auto licenseType = getProperty(ResourcePropertyKey::kForcedLicenseType);
    Qn::LicenseType result = nx::reflect::fromString(licenseType.toStdString(), Qn::LC_Professional);

    if (result != Qn::LicenseType::LC_Free
        && systemContext()->saasServiceManager()->saasState() != SaasState::uninitialized)
    {
        return Qn::LC_SaasLocalRecording;
    }

    return result;
}

QnVirtualCameraResource::MotionStreamIndex QnVirtualCameraResource::motionStreamIndex() const
{
    return m_cachedMotionStreamIndex.get();
}

QnVirtualCameraResource::MotionStreamIndex QnVirtualCameraResource::motionStreamIndexInternal() const
{
    const auto motionStream = nx::reflect::fromString<nx::vms::api::StreamIndex>(
        getProperty(ResourcePropertyKey::kMotionStreamKey).toLower().toStdString(),
        nx::vms::api::StreamIndex::undefined);

    if (motionStream == nx::vms::api::StreamIndex::undefined)
        return {nx::vms::api::StreamIndex::undefined, /*isForced*/ false};

    const bool forcedMotionDetection = QnLexical::deserialized<bool>(
        getProperty(ResourcePropertyKey::kForcedMotionDetectionKey).toLower(), true);

    return {motionStream, forcedMotionDetection};
}

void QnVirtualCameraResource::setMotionStreamIndex(MotionStreamIndex value)
{
    setProperty(ResourcePropertyKey::kMotionStreamKey,
        QString::fromStdString(nx::reflect::toString(value.index)));
    setProperty(
        ResourcePropertyKey::kForcedMotionDetectionKey, QnLexical::serialized(value.isForced));
    m_cachedMotionStreamIndex.reset();
}

bool QnVirtualCameraResource::isRemoteArchiveMotionDetectionEnabled() const
{
    const QString valueStr = getProperty(kRemoteArchiveMotionDetectionKey);
    return valueStr.isEmpty() || QnLexical::deserialized<bool>(valueStr, true);
}

void QnVirtualCameraResource::setRemoteArchiveMotionDetectionEnabled(bool value)
{
    const QString valueStr = value ? QString() : QnLexical::serialized(value);
    setProperty(kRemoteArchiveMotionDetectionKey, valueStr);
}

QnMotionRegion QnVirtualCameraResource::getMotionRegion() const
{
    auto motionList = getMotionRegionList();
    if (!motionList.empty())
        return motionList[0];

    return QnMotionRegion();
}

QList<QnMotionRegion> QnVirtualCameraResource::getMotionRegionList() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return parseMotionRegionList(m_userAttributes.motionMask).value_or(QList<QnMotionRegion>());
}

void QnVirtualCameraResource::setMotionRegionList(const QList<QnMotionRegion>& maskList)
{
    auto data = serializeMotionRegionList(maskList).toLatin1();
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.motionMask == data)
            return;

        m_userAttributes.motionMask = data;

        // TODO: This code does not work correctly when motion type is `default` or `none`.
        if (m_userAttributes.motionType != nx::vms::api::MotionType::software)
        {
            for (int i = 0; i < getVideoLayout()->channelCount(); ++i)
                setMotionMaskPhysical(i);
        }
    }
    emit motionRegionChanged(::toSharedPointer(this));
}

void QnVirtualCameraResource::setScheduleTasks(const QnScheduleTaskList& scheduleTasks)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.scheduleTasks == scheduleTasks)
            return;

        m_userAttributes.scheduleTasks = scheduleTasks;
    }
    emit scheduleTasksChanged(::toSharedPointer(this));
}

QnScheduleTaskList QnVirtualCameraResource::getScheduleTasks() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.scheduleTasks;
}

bool QnVirtualCameraResource::hasDualStreaming() const
{
    return m_cachedHasDualStreaming.get();
}

nx::vms::api::CameraMediaCapability QnVirtualCameraResource::cameraMediaCapability() const
{
    return m_cachedCameraMediaCapabilities.get();
}

void QnVirtualCameraResource::setCameraMediaCapability(const nx::vms::api::CameraMediaCapability& value)
{
    setProperty(
        ResourcePropertyKey::kMediaCapabilities, QString::fromLatin1(QJson::serialized(value)));
    m_cachedCameraMediaCapabilities.reset();
}

bool QnVirtualCameraResource::isDtsBased() const
{
    return m_cachedIsDtsBased.get() || enforcedDeviceType() == nx::vms::api::DeviceType::nvr;
}

bool QnVirtualCameraResource::supportsSchedule() const
{
    if (hasFlags(Qn::virtual_camera))
        return false;

    if (!hasVideo() && !isAudioSupported())
        return false;

    return !isDtsBased() || m_cachedCanConfigureRemoteRecording.get();
}

bool QnVirtualCameraResource::isAnalog() const
{
    QString val = getProperty(ResourcePropertyKey::kAnalog);
    return val.toInt() > 0;
}

bool QnVirtualCameraResource::isAnalogEncoder() const
{
    if (enforcedDeviceType() == nx::vms::api::DeviceType::encoder)
        return true;

    return resourceData().value<bool>(ResourceDataKey::kAnalogEncoder);
}

bool QnVirtualCameraResource::isOnvifDevice() const
{
    return hasCameraCapabilities(nx::vms::api::DeviceCapability::isOnvif);
}

bool QnVirtualCameraResource::isSharingLicenseInGroup() const
{
    if (getGroupId().isEmpty())
        return false; //< Not a multichannel device. Nothing to share
    if (!QnLicense::licenseTypeInfo(licenseType()).allowedToShareChannel)
        return false; //< Don't allow sharing for encoders e.t.c

    return resourceData().value<bool>(ResourceDataKey::kCanShareLicenseGroup, false);
}

bool QnVirtualCameraResource::isNvr() const
{
    return isDtsBased();
}

bool QnVirtualCameraResource::isMultiSensorCamera() const
{
    return !getGroupId().isEmpty()
           && !isDtsBased()
           && !isAnalogEncoder()
           && !isAnalog()
           && !nx::vms::api::isProxyDeviceType(enforcedDeviceType());
}

nx::vms::api::DeviceType QnVirtualCameraResource::enforcedDeviceType() const
{
    return m_cachedExplicitDeviceType.get();
}

void QnVirtualCameraResource::setDeviceType(nx::vms::api::DeviceType deviceType)
{
    m_cachedExplicitDeviceType.reset();
    m_cachedLicenseType.reset();
    setProperty(ResourcePropertyKey::kDeviceType,
        QString::fromStdString(nx::reflect::toString(deviceType)));
}

Qn::LicenseType QnVirtualCameraResource::licenseType() const
{
    return m_cachedLicenseType.get();
}

StreamFpsSharingMethod QnVirtualCameraResource::streamFpsSharingMethod() const
{
    const QString value = getProperty(ResourcePropertyKey::kStreamFpsSharing);
    return kFpsSharingMethodToString.key(value, kDefaultStreamFpsSharingMethod);
}

void QnVirtualCameraResource::setStreamFpsSharingMethod(StreamFpsSharingMethod value)
{
    setProperty(ResourcePropertyKey::kStreamFpsSharing, kFpsSharingMethodToString[value]);
}

bool QnVirtualCameraResource::setIoPortDescriptions(QnIOPortDataList newPorts, bool needMerge)
{
    const auto savedPorts = ioPortDescriptions();
    bool wasDataMerged = false;
    bool hasInputs = false;
    bool hasOutputs = false;
    for (auto& newPort: newPorts)
    {
        if (needMerge)
        {
            if (const auto savedPort = nx::utils::find_if(savedPorts,
                [&](const auto& port)
                {
                  if (port.id != newPort.id)
                      return false;

                  // Input and output ports can have same IDs, so lets distinguish them by type.
                  if (newPort.supportedPortTypes == Qn::IOPortType::PT_Unknown)
                      return port.portType == newPort.portType;
                  else
                      return port.supportedPortTypes == newPort.supportedPortTypes;
                }))
            {
                newPort = *savedPort;
                wasDataMerged = true;
            }
        }

        const auto supportedTypes = newPort.supportedPortTypes | newPort.portType;
        hasInputs |= supportedTypes.testFlag(Qn::PT_Input);
        hasOutputs |= supportedTypes.testFlag(Qn::PT_Output);
    }

    setProperty(
        ResourcePropertyKey::kIoSettings,
        QString::fromStdString(nx::reflect::json::serialize(newPorts)));
    setCameraCapability(nx::vms::api::DeviceCapability::inputPort, hasInputs);
    setCameraCapability(nx::vms::api::DeviceCapability::outputPort, hasOutputs);

    return wasDataMerged;
}

QnIOPortDataList QnVirtualCameraResource::ioPortDescriptions(Qn::IOPortType type) const
{
    auto ports = m_cachedIoPorts.get();

    if (type != Qn::PT_Unknown)
        nx::utils::erase_if(ports, [type](auto p) { return p.portType != type; });

    return ports;
}

bool QnVirtualCameraResource::isIntercom() const
{
    return m_isIntercom.get();
}

void QnVirtualCameraResource::at_motionRegionChanged()
{
    if (flags() & Qn::foreigner)
        return;

    if (getMotionType() == nx::vms::api::MotionType::hardware
        || getMotionType() == nx::vms::api::MotionType::window)
    {
        QnConstResourceVideoLayoutPtr layout = getVideoLayout();
        int numChannels = layout->channelCount();
        for (int i = 0; i < numChannels; ++i)
            setMotionMaskPhysical(i);
    }
}

int QnVirtualCameraResource::motionWindowCount() const
{
    QString val = getProperty(ResourcePropertyKey::kMotionWindowCnt);
    return val.toInt();
}

int QnVirtualCameraResource::motionMaskWindowCount() const
{
    QString val = getProperty(ResourcePropertyKey::kMotionMaskWindowCnt);
    return val.toInt();
}

int QnVirtualCameraResource::motionSensWindowCount() const
{
    QString val = getProperty(ResourcePropertyKey::kMotionSensWindowCnt);
    return val.toInt();
}

bool QnVirtualCameraResource::hasTwoWayAudio() const
{
    return hasCameraCapabilities(nx::vms::api::DeviceCapability::audioTransmit);
}

Ptz::Capabilities QnVirtualCameraResource::getPtzCapabilities(ptz::Type ptzType) const
{
    switch (ptzType)
    {
        case ptz::Type::operational:
            return Ptz::Capabilities(getProperty(
                ResourcePropertyKey::kPtzCapabilities).toInt());

        case ptz::Type::configurational:
            return Ptz::Capabilities(getProperty(
                ResourcePropertyKey::kConfigurationalPtzCapabilities).toInt());
        default:
            NX_ASSERT(false, "Wrong ptz type, we should never be here");
            return Ptz::NoPtzCapabilities;
    }
}

bool QnVirtualCameraResource::hasAnyOfPtzCapabilities(
    Ptz::Capabilities capabilities,
    ptz::Type ptzType) const
{
    return getPtzCapabilities(ptzType) & capabilities;
}

void QnVirtualCameraResource::setPtzCapabilities(
    Ptz::Capabilities capabilities,
    ptz::Type ptzType)
{
    switch (ptzType)
    {
        case ptz::Type::operational:
        {
            // TODO: #sivanov Change the storage type of PTZ capabilities to serialized lexical.
            setProperty(
                ResourcePropertyKey::kPtzCapabilities,
                QString::number((int) capabilities));
            return;
        }
        case ptz::Type::configurational:
        {
            // TODO: #sivanov Change the storage type of PTZ capabilities to serialized lexical.
            setProperty(
                ResourcePropertyKey::kConfigurationalPtzCapabilities,
                QString::number((int) capabilities));
            return;
        }
    }
    NX_ASSERT(false, "Wrong ptz type, we should never be here");
}

void QnVirtualCameraResource::setPtzCapability(
    Ptz::Capabilities capability,
    bool value,
    ptz::Type ptzType)
{
    setPtzCapabilities(value
        ? (getPtzCapabilities(ptzType) | capability)
        : (getPtzCapabilities(ptzType) & ~capability),
        ptzType);
}

bool QnVirtualCameraResource::canSwitchPtzPresetTypes() const
{
    const auto capabilities = getPtzCapabilities();
    if (!(capabilities & Ptz::NativePresetsPtzCapability))
        return false;

    if (capabilities & Ptz::NoNxPresetsPtzCapability)
        return false;

    // Check if our server can emulate presets.
    return (capabilities & Ptz::AbsolutePtrzCapabilities)
        && (capabilities & Ptz::PositioningPtzCapabilities);
}

bool QnVirtualCameraResource::isAudioSupported() const
{
    const auto capabilities = cameraMediaCapability();
    if (capabilities.hasAudio)
        return true;

    // Compatibility with version < 3.1.2
    QString val = getProperty(ResourcePropertyKey::kIsAudioSupported);
    return val.toInt() > 0;
}

nx::vms::api::MotionType QnVirtualCameraResource::getDefaultMotionType() const
{
    nx::vms::api::MotionTypes value = supportedMotionTypes();
    if (value.testFlag(nx::vms::api::MotionType::software))
        return nx::vms::api::MotionType::software;

    if (value.testFlag(nx::vms::api::MotionType::hardware))
        return nx::vms::api::MotionType::hardware;

    if (value.testFlag(nx::vms::api::MotionType::window))
        return nx::vms::api::MotionType::window;

    return nx::vms::api::MotionType::none;
}

nx::vms::api::MotionTypes QnVirtualCameraResource::supportedMotionTypes() const
{
    return m_cachedSupportedMotionTypes.get();
}

bool QnVirtualCameraResource::isMotionDetectionSupported() const
{
    nx::vms::api::MotionType motionType = getDefaultMotionType();

    // TODO: We can get an invalid result here if camera supports both software and hardware motion
    // detection, but there are no such cameras for now.
    if (motionType == nx::vms::api::MotionType::software)
    {
        return hasDualStreaming()
               || hasCameraCapabilities(nx::vms::api::DeviceCapability::primaryStreamSoftMotion)
               || motionStreamIndex().isForced;
    }
    return motionType != nx::vms::api::MotionType::none;
}

bool QnVirtualCameraResource::isMotionDetectionEnabled() const
{
    const auto motionType = getMotionType();
    return motionType != nx::vms::api::MotionType::none
           && supportedMotionTypes().testFlag(motionType);
}

nx::vms::api::MotionType QnVirtualCameraResource::getMotionType() const
{
    return m_motionType.get();
}

nx::vms::api::MotionType QnVirtualCameraResource::calculateMotionType() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    if (m_userAttributes.motionType == nx::vms::api::MotionType::none)
        return m_userAttributes.motionType;

    if (m_userAttributes.motionType == nx::vms::api::MotionType::default_
        || !(supportedMotionTypes().testFlag(m_userAttributes.motionType)))
    {
        return getDefaultMotionType();
    }

    return m_userAttributes.motionType;
}

nx::vms::api::BackupContentTypes QnVirtualCameraResource::getBackupContentType() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.backupContentType;
}

void QnVirtualCameraResource::setBackupContentType(nx::vms::api::BackupContentTypes contentTypes)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.backupContentType == contentTypes)
            return;

        m_userAttributes.backupContentType = contentTypes;
    }
    emit backupContentTypeChanged(::toSharedPointer(this));
}

nx::vms::api::BackupPolicy QnVirtualCameraResource::getBackupPolicy() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.backupPolicy;
}

void QnVirtualCameraResource::setBackupPolicy(nx::vms::api::BackupPolicy backupPolicy)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.backupPolicy == backupPolicy)
            return;

        m_userAttributes.backupPolicy = backupPolicy;
    }
    emit backupPolicyChanged(::toSharedPointer(this));
}

bool QnVirtualCameraResource::isBackupEnabled() const
{
    using namespace nx::vms::api;
    const auto backupPolicy = getBackupPolicy();
    return backupPolicy == BackupPolicy::byDefault
           ? systemContext()->globalSettings()->backupSettings().backupNewCameras
           : backupPolicy == BackupPolicy::on;
}

QString QnVirtualCameraResource::getUrl() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return nx::utils::Url::fromUserInput(m_url).toString();
}

QnVirtualCameraResource::MotionStreamIndex QnVirtualCameraResource::calculateMotionStreamIndex() const
{
    auto selectedMotionStream = motionStreamIndexInternal();
    if (selectedMotionStream.index == nx::vms::api::StreamIndex::undefined)
        selectedMotionStream.index = nx::vms::api::StreamIndex::secondary;

    if (selectedMotionStream.index == nx::vms::api::StreamIndex::secondary && !hasDualStreaming())
        selectedMotionStream.index = nx::vms::api::StreamIndex::primary;

    return selectedMotionStream;
}

void QnVirtualCameraResource::setMotionType(nx::vms::api::MotionType value)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.motionType == value)
            return;

        m_userAttributes.motionType = value;
    }
    m_motionType.reset();
    emit motionTypeChanged(::toSharedPointer(this));
}

nx::vms::api::DeviceCapabilities QnVirtualCameraResource::getCameraCapabilities() const
{
    return m_cachedCameraCapabilities.get();
}

bool QnVirtualCameraResource::hasCameraCapabilities(
    nx::vms::api::DeviceCapabilities capabilities) const
{
    return (getCameraCapabilities() & capabilities) == capabilities;
}

void QnVirtualCameraResource::setCameraCapabilities(nx::vms::api::DeviceCapabilities capabilities)
{
    // TODO: #sivanov Change capabilities storage type to serialized lexical.
    setProperty(ResourcePropertyKey::kCameraCapabilities, QString::number((int) capabilities));
    m_cachedCameraCapabilities.reset();
}

void QnVirtualCameraResource::setCameraCapability(
    nx::vms::api::DeviceCapability capability, bool value)
{
    setCameraCapabilities(getCameraCapabilities().setFlag(capability, value));
}

QString QnVirtualCameraResource::getUserDefinedName() const
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (!m_userAttributes.cameraName.isEmpty())
            return m_userAttributes.cameraName;
    }

    return QnVirtualCameraResource::getName();
}

QString QnVirtualCameraResource::getUserDefinedGroupName() const
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (!m_userAttributes.userDefinedGroupName.isEmpty())
            return m_userAttributes.userDefinedGroupName;
    }
    return getDefaultGroupName();
}

QString QnVirtualCameraResource::getDefaultGroupName() const
{
    SAFE(return m_groupName);
}

void QnVirtualCameraResource::setDefaultGroupName(const QString& value)
{
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        if(m_groupName == value)
            return;
        m_groupName = value;
    }
    emit groupNameChanged(::toSharedPointer(this));
}

void QnVirtualCameraResource::setUserDefinedGroupName(const QString& value)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.userDefinedGroupName == value)
            return;

        m_userAttributes.userDefinedGroupName = value;
    }
    emit groupNameChanged(::toSharedPointer(this));
}

QString QnVirtualCameraResource::getGroupId() const
{
    SAFE(return m_groupId)
}

void QnVirtualCameraResource::setGroupId(const QString& value)
{
    QString previousGroupId;
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        if(m_groupId == value)
            return;
        previousGroupId = m_groupId;
        m_groupId = value;
    }
    emit groupIdChanged(::toSharedPointer(this), previousGroupId);
}

QString QnVirtualCameraResource::getSharedId() const
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_groupId.isEmpty())
            return m_groupId;
    }

    return getPhysicalId();
}

QString QnVirtualCameraResource::getModel() const
{
    SAFE(return m_model)
}

void QnVirtualCameraResource::setModel(const QString &model)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_model = model;
    emit modelChanged(::toSharedPointer(this));
}

QString QnVirtualCameraResource::getFirmware() const
{
    return getProperty(ResourcePropertyKey::kFirmware);
}

void QnVirtualCameraResource::setFirmware(const QString &firmware)
{
    QString fixedFirmware;
    for (const QChar symbol: firmware)
    {
        if (symbol.isPrint())
            fixedFirmware.append(symbol);
    }
    setProperty(ResourcePropertyKey::kFirmware, fixedFirmware);
}

bool QnVirtualCameraResource::trustCameraTime() const
{
    return QnLexical::deserialized<bool>(getProperty(ResourcePropertyKey::kTrustCameraTime));
}

void QnVirtualCameraResource::setTrustCameraTime(bool value)
{
    setProperty(ResourcePropertyKey::kTrustCameraTime, boolToPropertyStr(value));
}

bool QnVirtualCameraResource::keepCameraTimeSettings() const
{
    return QnLexical::deserialized<bool>(
        getProperty(ResourcePropertyKey::kKeepCameraTimeSettings), true);
}

void QnVirtualCameraResource::setKeepCameraTimeSettings(bool value)
{
    setProperty(ResourcePropertyKey::kKeepCameraTimeSettings, boolToPropertyStr(value));
}

QString QnVirtualCameraResource::getVendor() const
{
    SAFE(return m_vendor)

    // This code is commented for a reason. We want to know if vendor is empty.
    //SAFE(if (!m_vendor.isEmpty()) return m_vendor)    //calculated on the server
    //
    //QnResourceTypePtr resourceType = qnResTypePool->getResourceType(getTypeId());
    //return resourceType ? resourceType->getManufacturer() : QString(); //estimated value
}

void QnVirtualCameraResource::setVendor(const QString& value)
{
    SAFE(m_vendor = value)
    emit vendorChanged(::toSharedPointer(this));
}

int QnVirtualCameraResource::logicalId() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.logicalId.toInt();
}

void QnVirtualCameraResource::setLogicalId(int value)
{
    QString valueStr = value > 0 ? QString::number(value) : QString();
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.logicalId == valueStr)
            return;

        m_userAttributes.logicalId = valueStr;
    }
    emit logicalIdChanged(::toSharedPointer(this));
}

void QnVirtualCameraResource::setMaxPeriod(std::chrono::seconds value)
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    m_userAttributes.maxArchivePeriodS = value;
}

std::chrono::seconds QnVirtualCameraResource::maxPeriod() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.maxArchivePeriodS;
}

void QnVirtualCameraResource::setPreferredServerId(const nx::Uuid& value)
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    m_userAttributes.preferredServerId = value;
}

nx::Uuid QnVirtualCameraResource::preferredServerId() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.preferredServerId;
}

void QnVirtualCameraResource::setRemoteArchiveSynchronizationEnabled(bool value)
{
    const QString stored = value == kRemoteArchiveSynchronizationEnabledByDefault
                           ? QString()
                           : QnLexical::serialized(value);
    setProperty(ResourcePropertyKey::kRemoteArchiveSynchronizationEnabled, stored);
}

bool QnVirtualCameraResource::isRemoteArchiveSynchronizationEnabled() const
{
    bool isEnabled = kRemoteArchiveSynchronizationEnabledByDefault;
    QnLexical::deserialize(
        getProperty(ResourcePropertyKey::kRemoteArchiveSynchronizationEnabled),
        &isEnabled);

    return isEnabled;
}

void QnVirtualCameraResource::setManualRemoteArchiveSynchronizationTriggered(bool isTriggered)
{
    QString value;
    QnLexical::serialize(isTriggered, &value);
    setProperty(ResourcePropertyKey::kManualRemoteArchiveSynchronizationTriggered, value);
}

bool QnVirtualCameraResource::isManualRemoteArchiveSynchronizationTriggered() const
{
    bool isTriggered = false;
    QnLexical::deserialize(
        getProperty(ResourcePropertyKey::kManualRemoteArchiveSynchronizationTriggered),
        &isTriggered);
    return isTriggered;
}

void QnVirtualCameraResource::updatePreferredServerId()
{
    if (preferredServerId().isNull())
        setPreferredServerId(getParentId());
}

void QnVirtualCameraResource::setMinPeriod(std::chrono::seconds value)
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    m_userAttributes.minArchivePeriodS = value;
}

std::chrono::seconds QnVirtualCameraResource::minPeriod() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.minArchivePeriodS;
}

int QnVirtualCameraResource::recordBeforeMotionSec() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.recordBeforeMotionSec;
}

void QnVirtualCameraResource::setRecordBeforeMotionSec(int value)
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    m_userAttributes.recordBeforeMotionSec = value;
}

int QnVirtualCameraResource::recordAfterMotionSec() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.recordAfterMotionSec;
}

void QnVirtualCameraResource::setRecordAfterMotionSec(int value)
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    m_userAttributes.recordAfterMotionSec = value;
}

void QnVirtualCameraResource::setScheduleEnabled(bool value)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.scheduleEnabled == value)
            return;

        m_userAttributes.scheduleEnabled = value;
    }
    emit scheduleEnabledChanged(::toSharedPointer(this));
}

bool QnVirtualCameraResource::isScheduleEnabled() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.scheduleEnabled;
}

Qn::FailoverPriority QnVirtualCameraResource::failoverPriority() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.failoverPriority;
}

void QnVirtualCameraResource::setFailoverPriority(Qn::FailoverPriority value)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.failoverPriority == value)
            return;

        m_userAttributes.failoverPriority = value;
    }
    emit failoverPriorityChanged(::toSharedPointer(this));
}

bool QnVirtualCameraResource::isAudioRequired() const
{
    return m_cachedAudioRequired.get();
}

bool QnVirtualCameraResource::isAudioForced() const
{
    return getProperty(ResourcePropertyKey::kForcedAudioStream).toInt() > 0;
}

bool QnVirtualCameraResource::isAudioEnabled() const
{
    if (isAudioForced())
        return true;

    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (!m_userAttributes.audioEnabled)
            return false;
    }

    // If the camera has external audio input device but it does not exist anymore, use default value.
    if (!audioInputDeviceId().isNull() && !cameraExists(resourcePool(), audioInputDeviceId()))
        return false;

    return true;
}

void QnVirtualCameraResource::setAudioEnabled(bool enabled)
{
    if (isAudioForced())
    {
        NX_ASSERT(false, "Enabled state of audio not changed for device with forced audio");
        return;
    }

    const auto audioWasEnabled = isAudioEnabled();

    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        m_userAttributes.audioEnabled = enabled;
    }

    if (!audioInputDeviceId().isNull() && !cameraExists(resourcePool(), audioInputDeviceId()))
        setAudioInputDeviceId({});

    if (audioWasEnabled != isAudioEnabled())
        emit audioEnabledChanged(::toSharedPointer(this));
}

nx::Uuid QnVirtualCameraResource::audioInputDeviceId() const
{
    return nx::Uuid::fromStringSafe(getProperty(ResourcePropertyKey::kAudioInputDeviceId));
}

void QnVirtualCameraResource::setAudioInputDeviceId(const nx::Uuid& deviceId)
{
    if (!NX_ASSERT(deviceId != getId(), "Only another device may act as audio input override"))
        return;

    if (!NX_ASSERT(!hasFlags(Qn::virtual_camera),
        "Audio input override cannot be set for a virtual camera"))
    {
        return;
    }

    if (deviceId == audioInputDeviceId())
        return;

    if (deviceId.isNull())
    {
        // Null QString is always stored instead of string representation of null nx::Uuid to
        // prevent false property change notifications.
        setProperty(ResourcePropertyKey::kAudioInputDeviceId, QString());
        return;
    }

    setProperty(ResourcePropertyKey::kAudioInputDeviceId, deviceId.toString());
}

bool QnVirtualCameraResource::isTwoWayAudioEnabled() const
{
    const auto enabled = QnLexical::deserialized(
        getProperty(ResourcePropertyKey::kTwoWayAudioEnabled), hasTwoWayAudio());

    // If has audio output device, but it is not exists any more use default value.
    auto audioOutputDeviceId = this->audioOutputDeviceId();
    return enabled
           && (audioOutputDeviceId.isNull() || cameraExists(resourcePool(), audioOutputDeviceId));
}

void QnVirtualCameraResource::setTwoWayAudioEnabled(bool enabled)
{
    if (!NX_ASSERT(!hasFlags(Qn::virtual_camera),
        "Two way audio isn't configurable for virtual cameras"))
    {
        return;
    }

    if (!audioOutputDeviceId().isNull() && !cameraExists(resourcePool(), audioOutputDeviceId()))
        setAudioOutputDeviceId({});

    if (enabled != isTwoWayAudioEnabled())
        setProperty(ResourcePropertyKey::kTwoWayAudioEnabled, QnLexical::serialized(enabled));
}

nx::Uuid QnVirtualCameraResource::audioOutputDeviceId() const
{
    return nx::Uuid::fromStringSafe(getProperty(ResourcePropertyKey::kAudioOutputDeviceId));
}

QnVirtualCameraResourcePtr QnVirtualCameraResource::audioOutputDevice() const
{
    const auto redirectedOutputCamera =
        resourcePool()->getResourceById<QnVirtualCameraResource>(audioOutputDeviceId());

    if (redirectedOutputCamera)
        return redirectedOutputCamera;

    return toSharedPointer(this);
}

void QnVirtualCameraResource::setAudioOutputDeviceId(const nx::Uuid& deviceId)
{
    if (!NX_ASSERT(deviceId != getId(), "Only another device may act as audio output override"))
        return;

    if (!NX_ASSERT(!hasFlags(Qn::virtual_camera),
        "Audio output override cannot be set for a virtual camera"))
    {
        return;
    }

    if (deviceId == audioOutputDeviceId())
        return;

    if (deviceId.isNull())
    {
        // Null QString is always stored instead of string representation of null nx::Uuid to prevent
        // false property change notifications.
        setProperty(ResourcePropertyKey::kAudioOutputDeviceId, QString());
        return;
    }

    setProperty(ResourcePropertyKey::kAudioOutputDeviceId, deviceId.toString());
}

bool QnVirtualCameraResource::isManuallyAdded() const
{
    return m_manuallyAdded;
}

void QnVirtualCameraResource::setManuallyAdded(bool value)
{
    m_manuallyAdded = value;
}

bool QnVirtualCameraResource::isDefaultAuth() const
{
    return hasCameraCapabilities(nx::vms::api::DeviceCapability::isDefaultPassword);
}

nx::vms::api::CameraBackupQuality QnVirtualCameraResource::getBackupQuality() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.backupQuality;
}

void QnVirtualCameraResource::setBackupQuality(nx::vms::api::CameraBackupQuality value)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.backupQuality == value)
            return;

        m_userAttributes.backupQuality = value;
    }
    emit backupQualityChanged(::toSharedPointer(this));
}

nx::vms::api::CameraBackupQuality QnVirtualCameraResource::getActualBackupQualities() const
{
    nx::vms::api::CameraBackupQuality result = getBackupQuality();
    if (result != nx::vms::api::CameraBackupQuality::CameraBackupDefault)
        return result;

    /* If backup is not configured on this camera, use 'Backup newly added cameras' value */
    return systemContext()->globalSettings()->backupSettings().quality;
}

bool QnVirtualCameraResource::cameraHotspotsEnabled() const
{
    const auto propertyValue = getProperty(ResourcePropertyKey::kCameraHotspotsEnabled);
    if (propertyValue.isEmpty())
        return false;

    return QnLexical::deserialized(propertyValue, false);
}

void QnVirtualCameraResource::setCameraHotspotsEnabled(bool enabled)
{
    if (enabled != cameraHotspotsEnabled())
        setProperty(ResourcePropertyKey::kCameraHotspotsEnabled, QnLexical::serialized(enabled));
}

nx::vms::common::CameraHotspotDataList QnVirtualCameraResource::cameraHotspots() const
{
    const auto hotspotsPropertyValue = getProperty(ResourcePropertyKey::kCameraHotspotsData);
    if (hotspotsPropertyValue.isEmpty())
        return {};

    const auto [hotspotsData, result] =
        nx::reflect::json::deserialize<nx::vms::common::CameraHotspotDataList>(
            hotspotsPropertyValue.toStdString());

    if (!NX_ASSERT(result, "Failed to deserialize camera hotspots: %1", result.errorDescription))
        return {};

    return hotspotsData;
}

void QnVirtualCameraResource::setCameraHotspots(
    const nx::vms::common::CameraHotspotDataList& hotspots)
{
    nx::vms::common::CameraHotspotDataList savedHotspots;

    std::copy_if(hotspots.cbegin(), hotspots.cend(), std::back_inserter(savedHotspots),
        [](const auto& hotspot) { return hotspot.isValid(); });

    for (auto& hotspot: savedHotspots)
        hotspot.fixupData();

    setProperty(ResourcePropertyKey::kCameraHotspotsData, !savedHotspots.empty()
                                                          ? QString::fromStdString(nx::reflect::json::serialize(savedHotspots))
                                                          : QString());
}

bool QnVirtualCameraResource::isDualStreamingDisabled() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return m_userAttributes.disableDualStreaming;
}

void QnVirtualCameraResource::setDisableDualStreaming(bool value)
{
    {
        NX_MUTEX_LOCKER lock(&m_attributesMutex);
        if (m_userAttributes.disableDualStreaming == value)
            return;

        m_userAttributes.disableDualStreaming = value;
    }
    m_cachedHasDualStreaming.reset();
    m_cachedMotionStreamIndex.reset();
    emit disableDualStreamingChanged(::toSharedPointer(this));
}

bool QnVirtualCameraResource::isPrimaryStreamRecorded() const
{
    const bool forbidden =
        getProperty(ResourcePropertyKey::kDontRecordPrimaryStreamKey).toInt() > 0;
    return !forbidden;
}

void QnVirtualCameraResource::setPrimaryStreamRecorded(bool value)
{
    const QString valueStr = value ? "" : "1";
    setProperty(ResourcePropertyKey::kDontRecordPrimaryStreamKey, valueStr);
}

bool QnVirtualCameraResource::isAudioRecorded() const
{
    const bool forbidden =
        getProperty(ResourcePropertyKey::kDontRecordAudio).toInt() > 0;
    return !forbidden;
}

void QnVirtualCameraResource::setRecordAudio(bool value)
{
    const QString valueStr = value ? "" : "1";
    setProperty(ResourcePropertyKey::kDontRecordAudio, valueStr);
}

bool QnVirtualCameraResource::isSecondaryStreamRecorded() const
{
    if (!hasDualStreamingInternal() && isDualStreamingDisabled())
        return false;

    const bool forbidden =
        getProperty(ResourcePropertyKey::kDontRecordSecondaryStreamKey).toInt() > 0;
    return !forbidden;
}

void QnVirtualCameraResource::setSecondaryStreamRecorded(bool value)
{
    if (!hasDualStreamingInternal() && isDualStreamingDisabled())
        return;

    const QString valueStr = value ? "" : "1";
    setProperty(ResourcePropertyKey::kDontRecordSecondaryStreamKey, valueStr);
}

void QnVirtualCameraResource::setCameraControlDisabled(bool value)
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    m_userAttributes.controlEnabled = !value;
}

bool QnVirtualCameraResource::isCameraControlDisabled() const
{
    if (const auto context = systemContext())
    {
        const auto& settings = context->globalSettings();
        if (settings && !settings->isCameraSettingsOptimizationEnabled())
            return true;
    }

    return isCameraControlDisabledInternal();
}

bool QnVirtualCameraResource::isCameraControlDisabledInternal() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    return !m_userAttributes.controlEnabled;
}

Qn::CameraStatusFlags QnVirtualCameraResource::statusFlags() const
{
    SAFE(return m_statusFlags)
}

bool QnVirtualCameraResource::hasStatusFlags(Qn::CameraStatusFlag value) const
{
    SAFE(return m_statusFlags & value)
}

void QnVirtualCameraResource::setStatusFlags(Qn::CameraStatusFlags value)
{
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        if(m_statusFlags == value)
            return;
        m_statusFlags = value;
    }
    emit statusFlagsChanged(::toSharedPointer(this));
}

void QnVirtualCameraResource::addStatusFlags(Qn::CameraStatusFlag flag)
{
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        Qn::CameraStatusFlags value = m_statusFlags | flag;
        if(m_statusFlags == value)
            return;
        m_statusFlags = value;
    }
    emit statusFlagsChanged(::toSharedPointer(this));
}

void QnVirtualCameraResource::removeStatusFlags(Qn::CameraStatusFlag flag)
{
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        Qn::CameraStatusFlags value = m_statusFlags & ~flag;
        if(m_statusFlags == value)
            return;
        m_statusFlags = value;
    }
    emit statusFlagsChanged(::toSharedPointer(this));
}

bool QnVirtualCameraResource::needCheckIpConflicts() const {
    return !hasCameraCapabilities(nx::vms::api::DeviceCapability::shareIp);
}

QnTimePeriodList QnVirtualCameraResource::getDtsTimePeriodsByMotionRegion(
    const QList<QRegion>& /*regions*/,
    qint64 /*msStartTime*/,
    qint64 /*msEndTime*/,
    int /*detailLevel*/,
    bool /*keepSmalChunks*/,
    int /*limit*/,
    Qt::SortOrder /*sortOrder*/)
{
    return QnTimePeriodList();
}

bool QnVirtualCameraResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source)
{
    QnVirtualCameraResource* camera = dynamic_cast<QnVirtualCameraResource*>(source.data());
    if (!camera)
        return false;

    bool result = base_type::mergeResourcesIfNeeded(source);
    const auto mergeValue =
        [&](auto getter, auto setter, auto emptinessChecker)
        {
          const auto newValue = (camera->*getter)();
          if (!emptinessChecker(newValue))
          {
              const auto currentValue = (this->*getter)();
              if (currentValue != newValue)
              {
                  NX_VERBOSE(this, "Merge resources. The value changed from %1 to %2", currentValue, newValue);
                  (this->*setter)(newValue);
                  result = true;
              }
          }
        };

    // Group id and name can be changed for any resource because if we unable to authorize,
    // number of channels is not accessible.
    mergeValue(
        &QnVirtualCameraResource::getGroupId,
        &QnVirtualCameraResource::setGroupId,
        isStringEmpty);
    mergeValue(
        &QnVirtualCameraResource::getDefaultGroupName,
        &QnVirtualCameraResource::setDefaultGroupName,
        isStringEmpty);
    mergeValue(
        &QnVirtualCameraResource::getModel,
        &QnVirtualCameraResource::setModel,
        isStringEmpty);
    mergeValue(
        &QnVirtualCameraResource::getVendor,
        &QnVirtualCameraResource::setVendor,
        isStringEmpty);
    mergeValue(
        &QnVirtualCameraResource::getFirmware,
        &QnVirtualCameraResource::setFirmware,
        isStringEmpty);
    mergeValue(
        &QnVirtualCameraResource::enforcedDeviceType,
        &QnVirtualCameraResource::setDeviceType,
        isDeviceTypeEmpty);
    return result;
}

nx::vms::api::MotionTypes QnVirtualCameraResource::calculateSupportedMotionTypes() const
{
    if (!hasVideo())
        return nx::vms::api::MotionType::none;

    QString val = getProperty(ResourcePropertyKey::kSupportedMotion);
    if (val.isEmpty())
        return nx::vms::api::MotionType::software;

    nx::vms::api::MotionTypes result = nx::vms::api::MotionType::default_;
    for (const QString& str: val.split(','))
    {
        QString motionType = str.toLower().trimmed();
        if (motionType == "hardwaregrid")
            result |= nx::vms::api::MotionType::hardware;
        else if (motionType == "softwaregrid")
            result |= nx::vms::api::MotionType::software;
        else if (motionType == "motionwindow")
            result |= nx::vms::api::MotionType::window;
    }

    return result;
}

void QnVirtualCameraResource::resetCachedValues()
{
    NX_VERBOSE(this, "Resetting all cached values");

    // TODO: #rvasilenko reset only required values on property changed (as in server resource).
    //resetting cached values
    m_cachedHasDualStreaming.reset();
    m_cachedSupportedMotionTypes.reset();
    m_cachedCameraCapabilities.reset();
    m_cachedIsDtsBased.reset();
    m_motionType.reset();
    m_cachedIsIOModule.reset();
    m_cachedCanConfigureRemoteRecording.reset();
    m_cachedCameraMediaCapabilities.reset();
    m_cachedLicenseType.reset();
    m_cachedExplicitDeviceType.reset();
    m_cachedMotionStreamIndex.reset();
    m_cachedIoPorts.reset();
    m_cachedHasVideo.reset();
    m_cachedBackupMegapixels.reset();
    m_cachedMediaStreams.reset();
    m_primaryStreamConfiguration.reset();
    m_secondaryStreamConfiguration.reset();
}

bool QnVirtualCameraResource::useBitratePerGop() const
{
    auto result = getProperty(ResourcePropertyKey::kBitratePerGOP);
    if (!result.isEmpty())
        return result.toInt() > 0;

    return resourceData().value<bool>(ResourceDataKey::kBitratePerGOP);
}

nx::core::resource::UsingOnvifMedia2Type QnVirtualCameraResource::useMedia2ToFetchProfiles() const
{
    const auto usingMedia2Type = nx::reflect::fromString(
        getProperty(ResourcePropertyKey::kUseMedia2ToFetchProfiles).toStdString(),
        nx::core::resource::UsingOnvifMedia2Type::autoSelect);

    return usingMedia2Type;
}

bool QnVirtualCameraResource::isIOModule() const
{
    return m_cachedIsIOModule.get();
}

nx::core::resource::AbstractRemoteArchiveManager* QnVirtualCameraResource::remoteArchiveManager()
{
    return nullptr;
}

float QnVirtualCameraResource::rawSuggestBitrateKbps(
    Qn::StreamQuality quality, QSize resolution, int fps, const QString& codec)
{
    return nx::vms::common::CameraBitrateCalculator::suggestBitrateForQualityKbps(
        quality,
        resolution,
        fps,
        codec);
}

bool QnVirtualCameraResource::captureEvent(const nx::vms::event::AbstractEventPtr& /*event*/)
{
    return false;
}

Qn::ConnectionRole QnVirtualCameraResource::toConnectionRole(nx::vms::api::StreamIndex index)
{
    return index == nx::vms::api::StreamIndex::primary
           ? Qn::CR_LiveVideo
           : Qn::CR_SecondaryLiveVideo;
}

nx::vms::api::StreamIndex QnVirtualCameraResource::toStreamIndex(Qn::ConnectionRole role)
{
    return role == Qn::CR_SecondaryLiveVideo
           ? nx::vms::api::StreamIndex::secondary
           : nx::vms::api::StreamIndex::primary;
}

nx::core::ptz::PresetType QnVirtualCameraResource::preferredPtzPresetType() const
{
    auto userPreference = userPreferredPtzPresetType();
    if (userPreference != nx::core::ptz::PresetType::undefined)
        return userPreference;

    return defaultPreferredPtzPresetType();
}

nx::core::ptz::PresetType QnVirtualCameraResource::userPreferredPtzPresetType() const
{
    return nx::reflect::fromString(
        getProperty(ResourcePropertyKey::kUserPreferredPtzPresetType).toStdString(),
        nx::core::ptz::PresetType::undefined);
}

void QnVirtualCameraResource::setUserPreferredPtzPresetType(nx::core::ptz::PresetType presetType)
{
    setProperty(ResourcePropertyKey::kUserPreferredPtzPresetType,
        presetType == nx::core::ptz::PresetType::undefined
        ? QString()
        : QString::fromStdString(nx::reflect::toString(presetType)));
}

nx::core::ptz::PresetType QnVirtualCameraResource::defaultPreferredPtzPresetType() const
{
    return nx::reflect::fromString(
        getProperty(ResourcePropertyKey::kDefaultPreferredPtzPresetType).toStdString(),
        nx::core::ptz::PresetType::native);
}

void QnVirtualCameraResource::setDefaultPreferredPtzPresetType(nx::core::ptz::PresetType presetType)
{
    setProperty(ResourcePropertyKey::kDefaultPreferredPtzPresetType,
        QString::fromStdString(nx::reflect::toString(presetType)));
}

Ptz::Capabilities QnVirtualCameraResource::ptzCapabilitiesUserIsAllowedToModify() const
{
    return nx::reflect::fromString(
        getProperty(ResourcePropertyKey::kPtzCapabilitiesUserIsAllowedToModify).toStdString(),
        Ptz::Capabilities(Ptz::NoPtzCapabilities));
}

void QnVirtualCameraResource::setPtzCapabilitesUserIsAllowedToModify(Ptz::Capabilities capabilities)
{
    setProperty(
        ResourcePropertyKey::kPtzCapabilitiesUserIsAllowedToModify,
        QString::fromStdString(nx::reflect::toString(capabilities)));
}

Ptz::Capabilities QnVirtualCameraResource::ptzCapabilitiesAddedByUser() const
{
    return nx::reflect::fromString<Ptz::Capabilities>(
        getProperty(ResourcePropertyKey::kPtzCapabilitiesAddedByUser).toStdString(),
        Ptz::NoPtzCapabilities);
}

void QnVirtualCameraResource::setPtzCapabilitiesAddedByUser(Ptz::Capabilities capabilities)
{
    setProperty(ResourcePropertyKey::kPtzCapabilitiesAddedByUser,
        QString::fromStdString(nx::reflect::toString(capabilities)));
}

QPointF QnVirtualCameraResource::storedPtzPanTiltSensitivity() const
{
    return QJson::deserialized<QPointF>(
        getProperty(ResourcePropertyKey::kPtzPanTiltSensitivity).toLatin1(),
        QPointF(Ptz::kDefaultSensitivity, 0.0 /*uniformity flag*/));
}

QPointF QnVirtualCameraResource::ptzPanTiltSensitivity() const
{
    const auto sensitivity = storedPtzPanTiltSensitivity();
    return sensitivity.y() > 0.0
           ? sensitivity
           : QPointF(sensitivity.x(), sensitivity.x());
}

bool QnVirtualCameraResource::isPtzPanTiltSensitivityUniform() const
{
    return storedPtzPanTiltSensitivity().y() <= 0.0;
}

void QnVirtualCameraResource::setPtzPanTiltSensitivity(const QPointF& value)
{
    setProperty(ResourcePropertyKey::kPtzPanTiltSensitivity, QString(QJson::serialized(QPointF(
        std::clamp(value.x(), Ptz::kMinimumSensitivity, Ptz::kMaximumSensitivity),
        value.y() > 0.0
        ? std::clamp(value.y(), Ptz::kMinimumSensitivity, Ptz::kMaximumSensitivity)
        : 0.0))));
}

void QnVirtualCameraResource::setPtzPanTiltSensitivity(const qreal& uniformValue)
{
    setPtzPanTiltSensitivity(QPointF(uniformValue, 0.0 /*uniformity flag*/));
}

bool QnVirtualCameraResource::isVideoQualityAdjustable() const
{
    const auto recordingParamsForbidden =
        [this]()
        {
          // This parameter can never be overridden. Just read the Resource type info.
          QnResourceTypePtr resourceType = qnResTypePool->getResourceType(getTypeId());
          return resourceType && resourceType->hasParam(ResourcePropertyKey::kNoRecordingParams);
        };

    return hasVideo()
           && !recordingParamsForbidden()
           && !hasCameraCapabilities(nx::vms::api::DeviceCapability::fixedQuality);
}

float QnVirtualCameraResource::suggestBitrateKbps(
    const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const
{
    if (streamParams.bitrateKbps > 0)
    {
        auto result = streamParams.bitrateKbps;
        const auto& streamCapabilities = cameraMediaCapability().streamCapabilities;
        if (auto f = streamCapabilities.find(toStreamIndex(role));
            f != streamCapabilities.cend() && f->second.maxBitrateKbps > 0)
        {
            result = qBound(f->second.minBitrateKbps, result, f->second.maxBitrateKbps);
        }

        return result;
    }
    return suggestBitrateForQualityKbps(
        streamParams.quality, streamParams.resolution, streamParams.fps, streamParams.codec, role);
}

int QnVirtualCameraResource::suggestBitrateForQualityKbps(Qn::StreamQuality quality,
    QSize resolution, int fps, const QString& codec, Qn::ConnectionRole role) const
{
    if (role == Qn::CR_Default)
        role = Qn::CR_LiveVideo;
    auto mediaCaps = cameraMediaCapability();
    auto streamCapability =
        [&]
        {
          auto f = mediaCaps.streamCapabilities.find(toStreamIndex(role));
          return f != mediaCaps.streamCapabilities.cend() ? f->second : nx::vms::api::CameraStreamCapability();
        }();

    return nx::vms::common::CameraBitrateCalculator::suggestBitrateForQualityKbps(
        quality,
        resolution,
        fps,
        codec,
        streamCapability,
        useBitratePerGop());
}

bool QnVirtualCameraResource::setCameraCredentialsSync(
    const QAuthenticator& /*auth*/, QString* outErrorString)
{
    if (outErrorString)
        *outErrorString = lit("Operation is not permitted.");
    return false;
}

nx::media::StreamEvent QnVirtualCameraResource::checkForErrors() const
{
    const auto capabilities = getCameraCapabilities();
    if (capabilities.testFlag(nx::vms::api::DeviceCapability::isDefaultPassword))
        return nx::media::StreamEvent::forbiddenWithDefaultPassword;
    if (capabilities.testFlag(nx::vms::api::DeviceCapability::isOldFirmware))
        return nx::media::StreamEvent::oldFirmware;
    return nx::media::StreamEvent::noEvent;
}

// TODO: #skolesnik Seems like it can be simplified.
QnResourceData QnVirtualCameraResource::resourceData() const
{
    if (const auto context = systemContext())
        return context->resourceDataPool()->data(toSharedPointer(this));
    return {};
}

nx::vms::api::ExtendedCameraOutputs QnVirtualCameraResource::extendedOutputs() const
{
    nx::vms::api::ExtendedCameraOutputs result{};
    for (const auto& port: ioPortDescriptions(Qn::IOPortType::PT_Output))
        result |= port.extendedCameraOutput();
    return result;
}

nx::vms::api::CameraAttributesData QnVirtualCameraResource::getUserAttributes() const
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    auto result = m_userAttributes;
    result.cameraId = getId();
    return result;
}

void QnVirtualCameraResource::setUserAttributes(const nx::vms::api::CameraAttributesData& attributes)
{
    NX_MUTEX_LOCKER lock(&m_attributesMutex);
    m_userAttributes = attributes;
    m_cachedDewarpingParams.reset();
}

void QnVirtualCameraResource::setUserAttributesAndNotify(
    const nx::vms::api::CameraAttributesData& attributes)
{
    auto originalAttributes = getUserAttributes();
    setUserAttributes(attributes);

    emit resourceChanged(::toSharedPointer(this));

    if (originalAttributes.cameraName != attributes.cameraName)
        emit nameChanged(::toSharedPointer(this));

    if (originalAttributes.userDefinedGroupName != attributes.userDefinedGroupName)
        emit groupNameChanged(::toSharedPointer(this));

    if (originalAttributes.dewarpingParams != attributes.dewarpingParams)
        emit mediaDewarpingParamsChanged(::toSharedPointer(this));

    if (originalAttributes.scheduleEnabled != attributes.scheduleEnabled)
        emit scheduleEnabledChanged(::toSharedPointer(this));

    if (originalAttributes.scheduleTasks != attributes.scheduleTasks)
        emit scheduleTasksChanged(::toSharedPointer(this));

    if (originalAttributes.motionMask != attributes.motionMask)
        emit motionRegionChanged(::toSharedPointer(this));

    if (originalAttributes.failoverPriority != attributes.failoverPriority)
        emit failoverPriorityChanged(::toSharedPointer(this));

    if (originalAttributes.backupQuality != attributes.backupQuality)
        emit backupQualityChanged(::toSharedPointer(this));

    if (originalAttributes.logicalId != attributes.logicalId)
        emit logicalIdChanged(::toSharedPointer(this));

    if (originalAttributes.audioEnabled != attributes.audioEnabled)
        emit audioEnabledChanged(::toSharedPointer(this));

    if (originalAttributes.motionType != attributes.motionType)
        emit motionTypeChanged(::toSharedPointer(this));

    if (originalAttributes.backupContentType != attributes.backupContentType)
        emit backupContentTypeChanged(::toSharedPointer(this));

    if (originalAttributes.backupPolicy != attributes.backupPolicy)
        emit backupPolicyChanged(::toSharedPointer(this));
}

bool QnVirtualCameraResource::isRtspMetatadaRequired() const
{
    return !m_cachedRtspMetadataDisabled.get();
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
    return m_cachedMediaStreams.get();
}

CameraMediaStreamInfo QnVirtualCameraResource::streamInfo(nx::vms::api::StreamIndex index) const
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
    const auto stream = streamInfo(nx::vms::api::StreamIndex::primary);
    QSize size = stream.getResolution();
    if (size.isEmpty())
    {
        // Trying to use size from secondary stream
        const auto secondary = streamInfo(nx::vms::api::StreamIndex::secondary);
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
    if (key == ResourcePropertyKey::kIoSettings)
    {
        m_isIntercom.reset();
        emit ioPortDescriptionsChanged(::toSharedPointer(this));
    }
    else if (key == ResourcePropertyKey::kNoVideoSupport)
    {
        m_cachedHasVideo.reset();
        m_cachedSupportedMotionTypes.reset();
        m_motionType.reset();
        emit motionTypeChanged(::toSharedPointer(this));
    }
    else if (key == ResourcePropertyKey::kPtzCapabilities)
    {
        emit ptzCapabilitiesChanged(::toSharedPointer(this));
    }
    else if (key == ResourcePropertyKey::kNoVideoSupport)
    {
        m_cachedSupportedObjectTypes.reset();
        emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
    }
    else if (key == kUserEnabledAnalyticsEnginesProperty)
    {
        m_cachedUserEnabledAnalyticsEngines.reset();
        m_cachedSupportedEventTypes.reset();
        m_cachedSupportedObjectTypes.reset();
        emit userEnabledAnalyticsEnginesChanged(toSharedPointer(this));
        emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
        emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
    }
    else if (key == kCompatibleAnalyticsEnginesProperty)
    {
        m_cachedCompatibleAnalyticsEngines.reset();
        m_cachedSupportedEventTypes.reset();
        m_cachedSupportedObjectTypes.reset();
        emit compatibleAnalyticsEnginesChanged(toSharedPointer(this));
        emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
        emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
    }
    else if (key == kDeviceAgentManifestsProperty)
    {
        m_cachedDeviceAgentManifests.reset();
        m_cachedSupportedEventTypes.reset();
        m_cachedSupportedObjectTypes.reset();
        emit deviceAgentManifestsChanged(toSharedPointer(this));
        emit compatibleEventTypesMaybeChanged(toSharedPointer(this));
        emit compatibleObjectTypesMaybeChanged(toSharedPointer(this));
    }
    else if (key == ResourcePropertyKey::kIoConfigCapability)
    {
        emit isIOModuleChanged(::toSharedPointer(this));
    }
    else if (key == ResourcePropertyKey::kMediaStreams)
    {
        m_cachedMediaStreams.reset();
        m_cachedBackupMegapixels.reset();
    }
    else if (key == ResourcePropertyKey::kPrimaryStreamConfiguration)
    {
        m_primaryStreamConfiguration.reset();
        m_cachedBackupMegapixels.reset();
    }
    else if (key == ResourcePropertyKey::kSecondaryStreamConfiguration)
    {
        m_secondaryStreamConfiguration.reset();
        m_cachedBackupMegapixels.reset();
    }

    if (key == kDeviceAgentManifestsProperty || key == kAnalyzedStreamIndexes)
    {
        auto cachedAnalyzedStreamIndex = m_cachedAnalyzedStreamIndex.lock();
        cachedAnalyzedStreamIndex->clear();
    }

    base_type::emitPropertyChanged(key, prevValue, newValue);
}

QSet<nx::Uuid> QnVirtualCameraResource::enabledAnalyticsEngines() const
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

QSet<nx::Uuid> QnVirtualCameraResource::userEnabledAnalyticsEngines() const
{
    return m_cachedUserEnabledAnalyticsEngines.get();
}

void QnVirtualCameraResource::setUserEnabledAnalyticsEngines(const QSet<nx::Uuid>& engines)
{
    const auto p = serializeUserEnabledAnalyticsEngines(engines);
    setProperty(p.name, p.value);
}

nx::vms::api::ResourceParamData QnVirtualCameraResource::serializeUserEnabledAnalyticsEngines(
    const QSet<nx::Uuid>& engines)
{
    return nx::vms::api::ResourceParamData(
        kUserEnabledAnalyticsEnginesProperty,
        QString::fromUtf8(QJson::serialized(engines))
    );
}

const QSet<nx::Uuid> QnVirtualCameraResource::compatibleAnalyticsEngines() const
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

void QnVirtualCameraResource::setCompatibleAnalyticsEngines(const QSet<nx::Uuid>& engines)
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
    const QSet<nx::Uuid>& engineIds)
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

QSet<nx::Uuid> QnVirtualCameraResource::calculateUserEnabledAnalyticsEngines() const
{
    return calculateUserEnabledAnalyticsEngines(getProperty(kUserEnabledAnalyticsEnginesProperty));
}

QSet<nx::Uuid> QnVirtualCameraResource::calculateUserEnabledAnalyticsEngines(const QString& value)
{
    return QJson::deserialized<QSet<nx::Uuid>>(value.toUtf8());
}

QSet<nx::Uuid> QnVirtualCameraResource::calculateCompatibleAnalyticsEngines() const
{
    return QJson::deserialized<QSet<nx::Uuid>>(
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
                    result.insert(supportedType.eventTypeId);
            }

            return result;
        });
}

AnalyticsEntitiesByEngine QnVirtualCameraResource::calculateSupportedObjectTypes() const
{
    if (!hasVideo())
        return {};

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
    QnVirtualCameraResource::deviceAgentManifests() const
{
    const auto data = getProperty(kDeviceAgentManifestsProperty).toStdString();
    if (data.empty())
        return {};

    auto [deserializedManifestMap, result] =
        nx::reflect::json::deserialize<DeviceAgentManifestMap>(data);

    if (!result.success)
    {
        // Trying to deserialize with another method to support client connections to
        // the old systems.
        bool deserialized = false;
        deserializedManifestMap = QJson::deserialized<DeviceAgentManifestMap>(
            data, {}, &deserialized);

        if (!deserialized)
            NX_WARNING(this, "Failed to deserialize manifest: %1", result.toString());
    }

    return deserializedManifestMap;
}

std::optional<nx::vms::api::analytics::DeviceAgentManifest>
    QnVirtualCameraResource::deviceAgentManifest(const nx::Uuid& engineId) const
{
    const auto manifests = m_cachedDeviceAgentManifests.get();
    auto it = manifests.find(engineId);
    if (it == manifests.cend())
        return std::nullopt;

    return it->second;
}

void QnVirtualCameraResource::setDeviceAgentManifest(
    const nx::Uuid& engineId,
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    auto manifests = m_cachedDeviceAgentManifests.get();
    manifests[engineId] = manifest;

    setProperty(
        kDeviceAgentManifestsProperty,
        QString::fromUtf8(nx::reflect::json::serialize(manifests)));
}

std::optional<nx::vms::api::StreamIndex>
    QnVirtualCameraResource::obtainUserChosenAnalyzedStreamIndex(nx::Uuid engineId) const
{
    using nx::vms::api::analytics::DeviceAgentManifest;

    const std::optional<DeviceAgentManifest> deviceAgentManifest =
        this->deviceAgentManifest(engineId);
    if (!deviceAgentManifest)
        return std::nullopt;

    if (deviceAgentManifest->capabilities.testFlag(
        nx::vms::api::analytics::DeviceAgentCapability::disableStreamSelection))
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
        return defaultAnalyzedStreamIndex();
    }

    if (const auto it = analyzedStreamIndexMap.find(engineId); it != analyzedStreamIndexMap.end())
        return *it;

    return std::nullopt;
}

nx::vms::api::StreamIndex QnVirtualCameraResource::analyzedStreamIndex(nx::Uuid engineId) const
{
    auto cachedAnalyzedStreamIndex = m_cachedAnalyzedStreamIndex.lock();
    auto it = cachedAnalyzedStreamIndex->find(engineId);
    if (it != cachedAnalyzedStreamIndex->end())
        return it->second;
    auto result = analyzedStreamIndexInternal(engineId);
    cachedAnalyzedStreamIndex->emplace(engineId, result);
    return result;
}

nx::vms::api::StreamIndex QnVirtualCameraResource::analyzedStreamIndexInternal(const nx::Uuid& engineId) const
{
    const std::optional<nx::vms::api::StreamIndex> userChosenAnalyzedStreamIndex =
        obtainUserChosenAnalyzedStreamIndex(engineId);
    if (userChosenAnalyzedStreamIndex)
        return userChosenAnalyzedStreamIndex.value();

    const QnResourcePool* const resourcePool = this->resourcePool();
    if (!NX_ASSERT(resourcePool))
        return defaultAnalyzedStreamIndex();

    const auto engineResource = resourcePool->getResourceById(
        engineId).dynamicCast<nx::vms::common::AnalyticsEngineResource>();
    if (!engineResource)
        return defaultAnalyzedStreamIndex();

    const nx::vms::api::analytics::EngineManifest manifest = engineResource->manifest();
    if (manifest.preferredStream != nx::vms::api::StreamIndex::undefined)
        return manifest.preferredStream;

    return defaultAnalyzedStreamIndex();
}

nx::vms::api::StreamIndex QnVirtualCameraResource::defaultAnalyzedStreamIndex() const
{
    return hasDualStreaming() ? nx::vms::api::StreamIndex::secondary : nx::vms::api::StreamIndex::primary;
}

void QnVirtualCameraResource::setAnalyzedStreamIndex(
    nx::Uuid engineId, nx::vms::api::StreamIndex streamIndex)
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

    return cameraMediaCapability().hasDualStreaming;
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

bool QnVirtualCameraResource::isMotionDetectionActive() const
{
    if (!isMotionDetectionEnabled())
        return false;

    if (getStatus() == nx::vms::api::ResourceStatus::unauthorized)
        return false;

    if (getMotionType() != nx::vms::api::MotionType::software)
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
        index == nx::vms::api::StreamIndex::primary
        ? ResourcePropertyKey::kForcedPrimaryProfile
        : ResourcePropertyKey::kForcedSecondaryProfile);
}

int QnVirtualCameraResource::backupMegapixels() const
{
    return m_cachedBackupMegapixels.get();
}

QnLiveStreamParams QnVirtualCameraResource::streamConfiguration(
    nx::vms::api::StreamIndex stream) const
{
    using namespace nx::vms::api;
    switch (stream)
    {
        case StreamIndex::primary:
            return m_primaryStreamConfiguration.get();
        case StreamIndex::secondary:
            return m_secondaryStreamConfiguration.get();
        default:
            return QnLiveStreamParams();
    }
}

int QnVirtualCameraResource::backupMegapixels(nx::vms::api::CameraBackupQuality quality) const
{
    using namespace nx::vms::api;

    int result = 0;

    const auto mediaStreams = this->mediaStreams().streams;

    auto getMediaResolution =
        [&mediaStreams](StreamIndex stream)
        {
            const int streamIndex = (int) stream;
            if (streamIndex < mediaStreams.size())
                return mediaStreams[streamIndex].getResolution();
            return QSize();
        };

    for (const auto& stream: {StreamIndex::primary, StreamIndex::secondary})
    {
        if (!isQualityEnabledForStream(quality, stream))
            continue;
        QSize targetResolution = streamConfiguration(stream).resolution;
        if (targetResolution.isEmpty())
            targetResolution = getMediaResolution(stream);
        if (targetResolution.isEmpty())
            continue;

        result = std::max(result, ResolutionData(targetResolution).megaPixels());
    }

    return result;
}
