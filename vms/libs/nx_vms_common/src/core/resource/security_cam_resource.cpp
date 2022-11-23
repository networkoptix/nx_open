// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "security_cam_resource.h"

#include <api/global_settings.h>
#include <api/model/api_ioport_data.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/resource_data.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <licensing/license.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/backup_settings.h>
#include <recording/time_period_list.h>
#include <utils/camera/camera_bitrate_calculator.h>
#include <utils/common/synctime.h>
#include <utils/crypt/symmetrical.h>

#define SAFE(expr) {NX_MUTEX_LOCKER lock( &m_mutex ); expr;}

using StreamFpsSharingMethod = QnSecurityCamResource::StreamFpsSharingMethod;

namespace {

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

bool isDeviceTypeEmpty(nx::core::resource::DeviceType value)
{
    return value == nx::core::resource::DeviceType::unknown;
};

QString boolToPropertyStr(bool value)
{
    return value ? lit("1") : lit("0");
}

bool cameraExists(QnResourcePool* resourcePool, const QnUuid& uuid)
{
    return resourcePool && resourcePool->getResourceById<QnSecurityCamResource>(uuid);
}

} // namespace

const Qn::LicenseType QnSecurityCamResource::kDefaultLicenseType = Qn::LC_Professional;

bool QnSecurityCamResource::MotionStreamIndex::operator==(const MotionStreamIndex& other) const
{
    return index == other.index && isForced == other.isForced;
}

bool QnSecurityCamResource::MotionStreamIndex::operator!=(const MotionStreamIndex& other) const
{
    return !(*this == other);
}

QnUuid QnSecurityCamResource::makeCameraIdFromUniqueId(const QString& uniqueId)
{
    // ATTENTION: This logic is similar to the one in nx::vms::api::CameraData::fillId().
    if (uniqueId.isEmpty())
        return QnUuid();
    return guidFromArbitraryData(uniqueId);
}

void QnSecurityCamResource::setCommonModule(QnCommonModule* commonModule)
{
    if (this->commonModule())
        return;

    base_type::setCommonModule(commonModule);
    connect(commonModule->resourceDataPool(), &QnResourceDataPool::changed, this,
        &QnSecurityCamResource::resetCachedValues, Qt::DirectConnection);
}

QnSecurityCamResource::QnSecurityCamResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_recActionCnt(0),
    m_manuallyAdded(false),
    m_cachedAudioRequired(
        [this] {
            if (isAudioEnabled())
                return true;

            return this->commonModule()->resourcePropertyDictionary()->hasProperty(
                ResourcePropertyKey::kAudioInputDeviceId, getId().toString());
        }),
    m_cachedLicenseType([this] { return calculateLicenseType(); }),
    m_cachedHasDualStreaming(
        [this]()->bool
        {
            return hasDualStreamingInternal()
                && !isDualStreamingDisabled();
        }),
    m_cachedSupportedMotionTypes(
        std::bind(&QnSecurityCamResource::calculateSupportedMotionTypes, this)),
    m_cachedCameraCapabilities(
        [this]()
        {
            return static_cast<nx::vms::api::DeviceCapabilities>(
                getProperty(ResourcePropertyKey::kCameraCapabilities).toInt());
        }),
    m_cachedIsDtsBased(
        [this]()->bool{ return getProperty(ResourcePropertyKey::kDts).toInt() > 0; }),
    m_motionType(std::bind(&QnSecurityCamResource::calculateMotionType, this)),
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
            return QJson::deserialized<nx::media::CameraMediaCapability>(
                getProperty(ResourcePropertyKey::kMediaCapabilities).toUtf8());
        }),
    m_cachedExplicitDeviceType(
        [this]()
        {
            return nx::reflect::fromString(
                getProperty(ResourcePropertyKey::kDeviceType).toStdString(),
                nx::core::resource::DeviceType::unknown);
        }),
    m_cachedHasVideo(
        [this]() { return !resourceData().value(ResourceDataKey::kNoVideoSupport, false); }),
    m_cachedMotionStreamIndex([this]{ return calculateMotionStreamIndex(); })
{
    NX_VERBOSE(this, "Creating");
    addFlags(Qn::live_cam);

    connect(this, &QnResource::resourceChanged, this, &QnSecurityCamResource::resetCachedValues,
        Qt::DirectConnection);
    connect(this, &QnResource::propertyChanged, this, &QnSecurityCamResource::resetCachedValues,
        Qt::DirectConnection);
    connect(
        this, &QnSecurityCamResource::licenseTypeChanged,
        this, &QnSecurityCamResource::resetCachedValues,
        Qt::DirectConnection);
    connect(
        this, &QnSecurityCamResource::parentIdChanged,
        this, &QnSecurityCamResource::resetCachedValues,
        Qt::DirectConnection);
    connect(
        this, &QnSecurityCamResource::motionRegionChanged,
        this, &QnSecurityCamResource::at_motionRegionChanged,
        Qt::DirectConnection);

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
        this, &QnSecurityCamResource::audioEnabledChanged,
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
        });

    QnMediaResource::initMediaResource();
}

nx::core::resource::DeviceType QnSecurityCamResource::deviceType() const
{
    // TODO: remove all setters (setIoModule, setIsDtsBased e.t.c) and keep setDeviceType only
    using namespace nx::core::resource;
    if (const auto result = enforcedDeviceType(); result != DeviceType::unknown)
        return result;
    if (isDtsBased())
        return DeviceType::nvr;
    if (isIOModule())
        return DeviceType::ioModule;
    if (isAnalogEncoder() || isAnalog())
        return DeviceType::encoder;
    if (isMultiSensorCamera())
        return DeviceType::multisensorCamera;
    if (hasVideo())
        return DeviceType::camera;
    if (hasTwoWayAudio())
        return DeviceType::hornSpeaker;
    return DeviceType::unknown;
}

QnMediaServerResourcePtr QnSecurityCamResource::getParentServer() const {
    return getParentResource().dynamicCast<QnMediaServerResource>();
}

bool QnSecurityCamResource::setProperty(
    const QString &key,
    const QString &value,
    PropertyOptions options)
{
    return QnResource::setProperty(key, value, options);
}

bool QnSecurityCamResource::setProperty(
    const QString &key,
    const QVariant& value,
    PropertyOptions options)
{
    return QnResource::setProperty(key, value, options);
}

bool QnSecurityCamResource::isGroupPlayOnly() const {
    return hasDefaultProperty(ResourcePropertyKey::kGroupPlayParamName);
}

bool QnSecurityCamResource::needsToChangeDefaultPassword() const
{
    return isDefaultAuth()
        && hasCameraCapabilities(nx::vms::api::DeviceCapability::setUserPassword);
}

const QnResource* QnSecurityCamResource::toResource() const {
    return this;
}

QnResource* QnSecurityCamResource::toResource()
{
    return this;
}

const QnResourcePtr QnSecurityCamResource::toResourcePtr() const {
    return toSharedPointer();
}

QnResourcePtr QnSecurityCamResource::toResourcePtr()
{
    return toSharedPointer();
}

QnSecurityCamResource::~QnSecurityCamResource()
{
}

void QnSecurityCamResource::updateAudioRequiredOnDevice(const QString& deviceId)
{
    auto prevDevice = QnUuid(deviceId);
    if (!prevDevice.isNull())
    {
        auto resource = commonModule()->resourcePool()->getResourceById<QnSecurityCamResource>(
            prevDevice);
        if (resource)
            resource->updateAudioRequired();
    }
}

void QnSecurityCamResource::updateAudioRequired()
{
    auto prevValue = m_cachedAudioRequired.get();
    m_cachedAudioRequired.update();
    if (prevValue != m_cachedAudioRequired.get())
        emit audioRequiredChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    base_type::updateInternal(other, notifiers);

    QnSecurityCamResourcePtr other_casted = qSharedPointerDynamicCast<QnSecurityCamResource>(other);
    if (other_casted)
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

int QnSecurityCamResource::getMaxFps(StreamIndex streamIndex) const
{
    const auto capabilities = cameraMediaCapability();
    int result = capabilities.streamCapabilities.value(streamIndex).maxFps;
    if (result > 0)
        return result;

    // Compatibility with version < 3.1.2
    QString value = getProperty(ResourcePropertyKey::kMaxFps);
    return value.isNull() ? kDefaultMaxFps : value.toInt();
}

void QnSecurityCamResource::setMaxFps(int fps, StreamIndex streamIndex)
{
    nx::media::CameraMediaCapability capability = cameraMediaCapability();
    capability.streamCapabilities[streamIndex].maxFps = fps;
    setCameraMediaCapability(capability);
}

int QnSecurityCamResource::reservedSecondStreamFps() const
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

void QnSecurityCamResource::initializationDone()
{
    QnNetworkResource::initializationDone();
    resetCachedValues();
}

bool QnSecurityCamResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return m_cachedHasVideo.get();
}

Qn::LicenseType QnSecurityCamResource::calculateLicenseType() const
{
    const auto licenseType = getProperty(ResourcePropertyKey::kForcedLicenseType);
    return nx::reflect::fromString(licenseType.toStdString(), kDefaultLicenseType);
}

QList<QnMotionRegion> QnSecurityCamResource::getMotionRegionList() const
{
    NX_ASSERT(!getId().isNull());
    return userAttributesPool()->motionRegions(getIdForUserAttributes());
}

QRegion QnSecurityCamResource::getMotionMask(int channel) const {
    return getMotionRegion(channel).getMotionMask();
}

QnSecurityCamResource::MotionStreamIndex QnSecurityCamResource::motionStreamIndex() const
{
    return m_cachedMotionStreamIndex.get();
}

QnSecurityCamResource::MotionStreamIndex QnSecurityCamResource::motionStreamIndexInternal() const
{
    const auto motionStream = nx::reflect::fromString<StreamIndex>(
        getProperty(ResourcePropertyKey::kMotionStreamKey).toLower().toStdString(),
        StreamIndex::undefined);

    if (motionStream == StreamIndex::undefined)
        return {StreamIndex::undefined, /*isForced*/ false};

    const bool forcedMotionDetection = QnLexical::deserialized<bool>(
        getProperty(ResourcePropertyKey::kForcedMotionDetectionKey).toLower(), true);

    return {motionStream, forcedMotionDetection};
}

void QnSecurityCamResource::setMotionStreamIndex(MotionStreamIndex value)
{
    setProperty(ResourcePropertyKey::kMotionStreamKey,
        QString::fromStdString(nx::reflect::toString(value.index)));
    setProperty(
        ResourcePropertyKey::kForcedMotionDetectionKey, QnLexical::serialized(value.isForced));
    m_cachedMotionStreamIndex.reset();
}

bool QnSecurityCamResource::isRemoteArchiveMotionDetectionEnabled() const
{
    const QString valueStr = getProperty(kRemoteArchiveMotionDetectionKey);
    return valueStr.isEmpty() || QnLexical::deserialized<bool>(valueStr, true);
}

void QnSecurityCamResource::setRemoteArchiveMotionDetectionEnabled(bool value)
{
    const QString valueStr = value ? QString() : QnLexical::serialized(value);
    setProperty(kRemoteArchiveMotionDetectionKey, valueStr);
}

QnMotionRegion QnSecurityCamResource::getMotionRegion(int channel) const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    const auto& regions = userAttributesPool()->motionRegions(getIdForUserAttributes());
    if (regions.size() > channel)
        return regions[channel];

    return QnMotionRegion();
}

void QnSecurityCamResource::setMotionRegionList(const QList<QnMotionRegion>& maskList)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    MotionType motionType = MotionType::default_;
    if (!userAttributesPool()->setMotionRegions(getIdForUserAttributes(), maskList))
        return;

    motionType = userAttributesPool()->motionType(getIdForUserAttributes());

    // TODO: This code does not work correctly when motion type is `default` or `none`.
    if (motionType != MotionType::software)
    {
        for (int i = 0; i < getVideoLayout()->channelCount(); ++i)
            setMotionMaskPhysical(i);
    }

    emit motionRegionChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setScheduleTasks(const QnScheduleTaskList& scheduleTasks)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (userAttributesPool()->setScheduleTasks(getIdForUserAttributes(), scheduleTasks))
        emit scheduleTasksChanged(::toSharedPointer(this));
}

QnScheduleTaskList QnSecurityCamResource::getScheduleTasks() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->scheduleTasks(getIdForUserAttributes());
}

QnCameraUserAttributesPtr QnSecurityCamResource::userAttributes() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->getCopy(getIdForUserAttributes());
}

bool QnSecurityCamResource::hasDualStreaming() const {
    return m_cachedHasDualStreaming.get();
}

nx::media::CameraMediaCapability QnSecurityCamResource::cameraMediaCapability() const
{
    return m_cachedCameraMediaCapabilities.get();
}

void QnSecurityCamResource::setCameraMediaCapability(const nx::media::CameraMediaCapability& value)
{
    setProperty(
        ResourcePropertyKey::kMediaCapabilities, QString::fromLatin1(QJson::serialized(value)));
    m_cachedCameraMediaCapabilities.reset();
}

bool QnSecurityCamResource::hasDualStreamingInternal() const
{
    const auto capabilities = cameraMediaCapability();
    if (capabilities.hasDualStreaming)
        return true;

    // Compatibility with version < 3.1.2
    QString val = getProperty(ResourcePropertyKey::kHasDualStreaming);
    return val.toInt() > 0;
}

bool QnSecurityCamResource::isDtsBased() const
{
    return m_cachedIsDtsBased.get() || enforcedDeviceType() == nx::core::resource::DeviceType::nvr;
}

bool QnSecurityCamResource::supportsSchedule() const
{
    if (hasFlags(Qn::virtual_camera))
        return false;

    if (!hasVideo() && !isAudioSupported())
        return false;

    return !isDtsBased() || m_cachedCanConfigureRemoteRecording.get();
}

bool QnSecurityCamResource::isAnalog() const
{
    QString val = getProperty(ResourcePropertyKey::kAnalog);
    return val.toInt() > 0;
}

bool QnSecurityCamResource::isAnalogEncoder() const
{
    if (enforcedDeviceType() == nx::core::resource::DeviceType::encoder)
        return true;

    return resourceData().value<bool>(lit("analogEncoder"));
}

bool QnSecurityCamResource::isOnvifDevice() const
{
    return hasCameraCapabilities(nx::vms::api::DeviceCapability::isOnvif);
}

bool QnSecurityCamResource::isSharingLicenseInGroup() const
{
    if (getGroupId().isEmpty())
        return false; //< Not a multichannel device. Nothing to share
    if (!QnLicense::licenseTypeInfo(licenseType()).allowedToShareChannel)
        return false; //< Don't allow sharing for encoders e.t.c

    return resourceData().value<bool>(ResourceDataKey::kCanShareLicenseGroup, false);
}

bool QnSecurityCamResource::isNvr() const
{
    return isDtsBased();
}

bool QnSecurityCamResource::isMultiSensorCamera() const
{
    return !getGroupId().isEmpty()
        && !isDtsBased()
        && !isAnalogEncoder()
        && !isAnalog()
        && !nx::core::resource::isProxyDeviceType(enforcedDeviceType());
}

nx::core::resource::DeviceType QnSecurityCamResource::enforcedDeviceType() const
{
    return m_cachedExplicitDeviceType.get();
}

void QnSecurityCamResource::setDeviceType(nx::core::resource::DeviceType deviceType)
{
    m_cachedExplicitDeviceType.reset();
    m_cachedLicenseType.reset();
    setProperty(ResourcePropertyKey::kDeviceType,
        QString::fromStdString(nx::reflect::toString(deviceType)));
}

Qn::LicenseType QnSecurityCamResource::licenseType() const
{
    return m_cachedLicenseType.get();
}

StreamFpsSharingMethod QnSecurityCamResource::streamFpsSharingMethod() const
{
    const QString value = getProperty(ResourcePropertyKey::kStreamFpsSharing);
    return kFpsSharingMethodToString.key(value, kDefaultStreamFpsSharingMethod);
}

void QnSecurityCamResource::setStreamFpsSharingMethod(StreamFpsSharingMethod value)
{
    setProperty(ResourcePropertyKey::kStreamFpsSharing, kFpsSharingMethodToString[value]);
}

bool QnSecurityCamResource::setIoPortDescriptions(QnIOPortDataList newPorts, bool needMerge)
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

    setProperty(ResourcePropertyKey::kIoSettings, QString::fromUtf8(QJson::serialized(newPorts)));
    setCameraCapability(nx::vms::api::DeviceCapability::inputPort, hasInputs);
    setCameraCapability(nx::vms::api::DeviceCapability::outputPort, hasOutputs);
    return wasDataMerged;
}

QnIOPortDataList QnSecurityCamResource::ioPortDescriptions(Qn::IOPortType type) const
{
    auto ports = QJson::deserialized<QnIOPortDataList>(
        getProperty(ResourcePropertyKey::kIoSettings).toUtf8());

    if (type != Qn::PT_Unknown)
        nx::utils::remove_if(ports, [&](auto p) { return p.portType != type; });

    return ports;
}

void QnSecurityCamResource::at_motionRegionChanged()
{
    if (flags() & Qn::foreigner)
        return;

    if (getMotionType() == MotionType::hardware
        || getMotionType() == MotionType::window)
    {
        QnConstResourceVideoLayoutPtr layout = getVideoLayout();
        int numChannels = layout->channelCount();
        for (int i = 0; i < numChannels; ++i)
            setMotionMaskPhysical(i);
    }
}

int QnSecurityCamResource::motionWindowCount() const
{
    QString val = getProperty(ResourcePropertyKey::kMotionWindowCnt);
    return val.toInt();
}

int QnSecurityCamResource::motionMaskWindowCount() const
{
    QString val = getProperty(ResourcePropertyKey::kMotionMaskWindowCnt);
    return val.toInt();
}

int QnSecurityCamResource::motionSensWindowCount() const
{
    QString val = getProperty(ResourcePropertyKey::kMotionSensWindowCnt);
    return val.toInt();
}

bool QnSecurityCamResource::hasTwoWayAudio() const
{
    return hasCameraCapabilities(nx::vms::api::DeviceCapability::audioTransmit);
}

bool QnSecurityCamResource::isAudioSupported() const
{
    const auto capabilities = cameraMediaCapability();
    if (capabilities.hasAudio)
        return true;

    // Compatibility with version < 3.1.2
    QString val = getProperty(ResourcePropertyKey::kIsAudioSupported);
    return val.toInt() > 0;
}

nx::vms::api::MotionType QnSecurityCamResource::getDefaultMotionType() const
{
    MotionTypes value = supportedMotionTypes();
    if (value.testFlag(MotionType::software))
        return MotionType::software;

    if (value.testFlag(MotionType::hardware))
        return MotionType::hardware;

    if (value.testFlag(MotionType::window))
        return MotionType::window;

    return MotionType::none;
}

nx::vms::api::MotionTypes QnSecurityCamResource::supportedMotionTypes() const
{
    return m_cachedSupportedMotionTypes.get();
}

bool QnSecurityCamResource::isMotionDetectionSupported() const
{
    MotionType motionType = getDefaultMotionType();

    // TODO: We can get an invalid result here if camera supports both software and hardware motion
    // detection, but there are no such cameras for now.
    if (motionType == MotionType::software)
    {
        return hasDualStreaming()
            || hasCameraCapabilities(nx::vms::api::DeviceCapability::primaryStreamSoftMotion)
            || motionStreamIndex().isForced;
    }
    return motionType != MotionType::none;
}

bool QnSecurityCamResource::isMotionDetectionEnabled() const
{
    const auto motionType = getMotionType();
    return motionType != MotionType::none
        && supportedMotionTypes().testFlag(motionType);
}

nx::vms::api::MotionType QnSecurityCamResource::getMotionType() const
{
    return m_motionType.get();
}

nx::vms::api::MotionType QnSecurityCamResource::calculateMotionType() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    MotionType value = userAttributesPool()->motionType(getIdForUserAttributes());
    if (value == MotionType::none)
        return value;

    if (value == MotionType::default_
        || !(supportedMotionTypes().testFlag(value)))
    {
        return getDefaultMotionType();
    }

    return value;
}

nx::vms::api::BackupContentTypes QnSecurityCamResource::getBackupContentType() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->backupContentType(getIdForUserAttributes());
}

void QnSecurityCamResource::setBackupContentType(nx::vms::api::BackupContentTypes contentTypes)
{
    if (userAttributesPool()->setBackupContentType(getIdForUserAttributes(), contentTypes))
        emit backupContentTypeChanged(::toSharedPointer(this));
}

nx::vms::api::BackupPolicy QnSecurityCamResource::getBackupPolicy() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->backupPolicy(getIdForUserAttributes());
}

void QnSecurityCamResource::setBackupPolicy(nx::vms::api::BackupPolicy backupPolicy)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (userAttributesPool()->setBackupPolicy(getIdForUserAttributes(), backupPolicy))
        emit backupPolicyChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::isBackupEnabled() const
{
    using namespace nx::vms::api;
    const auto backupPolicy = getBackupPolicy();
    return backupPolicy == BackupPolicy::byDefault
        ? commonModule()->globalSettings()->backupSettings().backupNewCameras
        : backupPolicy == BackupPolicy::on;
}

QnSecurityCamResource::MotionStreamIndex QnSecurityCamResource::calculateMotionStreamIndex() const
{
    auto selectedMotionStream = motionStreamIndexInternal();
    if (selectedMotionStream.index == StreamIndex::undefined)
        selectedMotionStream.index = StreamIndex::secondary;

    if (selectedMotionStream.index == StreamIndex::secondary && !hasDualStreaming())
        selectedMotionStream.index = StreamIndex::primary;

    return selectedMotionStream;
}

void QnSecurityCamResource::setMotionType(MotionType value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (userAttributesPool()->setMotionType(getIdForUserAttributes(), value))
    {
        m_motionType.reset();
        emit motionTypeChanged(::toSharedPointer(this));
    }
}

nx::vms::api::DeviceCapabilities QnSecurityCamResource::getCameraCapabilities() const
{
    return m_cachedCameraCapabilities.get();
}

bool QnSecurityCamResource::hasCameraCapabilities(
    nx::vms::api::DeviceCapabilities capabilities) const
{
    return (getCameraCapabilities() & capabilities) == capabilities;
}

void QnSecurityCamResource::setCameraCapabilities(nx::vms::api::DeviceCapabilities capabilities)
{
    setProperty(ResourcePropertyKey::kCameraCapabilities, static_cast<int>(capabilities));
    m_cachedCameraCapabilities.reset();
}

void QnSecurityCamResource::setCameraCapability(
    nx::vms::api::DeviceCapability capability, bool value)
{
    setCameraCapabilities(getCameraCapabilities().setFlag(capability, value));
}

bool QnSecurityCamResource::isRecordingEventAttached() const
{
    return m_recActionCnt > 0;
}

void QnSecurityCamResource::recordingEventAttached()
{
    m_recActionCnt++;
    emit recordingActionChanged(toSharedPointer(this));
}

void QnSecurityCamResource::recordingEventDetached()
{
    m_recActionCnt = qMax(0, m_recActionCnt-1);
    emit recordingActionChanged(toSharedPointer(this));
}

QString QnSecurityCamResource::getUserDefinedName() const
{
    if (!getIdForUserAttributes().isNull() && commonModule())
    {
        auto name = userAttributesPool()->name(getIdForUserAttributes());
        if (!name.isEmpty())
            return name;
    }

    return QnSecurityCamResource::getName();
}

QString QnSecurityCamResource::getUserDefinedGroupName() const
{
    if (!getId().isNull() && commonModule())
    {
        auto groupName = userAttributesPool()->groupName(getIdForUserAttributes());
        if (!groupName.isEmpty())
            return groupName;
    }

    return getDefaultGroupName();
}

QString QnSecurityCamResource::getDefaultGroupName() const
{
    SAFE(return m_groupName);
}

void QnSecurityCamResource::setDefaultGroupName(const QString& value)
{
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        if(m_groupName == value)
            return;
        m_groupName = value;
    }
    emit groupNameChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setUserDefinedGroupName(const QString& value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (userAttributesPool()->setGroupName(getIdForUserAttributes(), value))
        emit groupNameChanged(::toSharedPointer(this));
}

QString QnSecurityCamResource::getGroupId() const
{
    SAFE(return m_groupId)
}

void QnSecurityCamResource::setGroupId(const QString& value)
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

QString QnSecurityCamResource::getSharedId() const
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_groupId.isEmpty())
            return m_groupId;
    }

    return getUniqueId();
}

QString QnSecurityCamResource::getModel() const
{
    SAFE(return m_model)
}

void QnSecurityCamResource::setModel(const QString &model)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_model = model;
    emit modelChanged(::toSharedPointer(this));
}

QString QnSecurityCamResource::getFirmware() const
{
    return getProperty(ResourcePropertyKey::kFirmware);
}

void QnSecurityCamResource::setFirmware(const QString &firmware)
{
    QString fixedFirmware;
    for (const QChar symbol: firmware)
    {
        if (symbol.isPrint())
            fixedFirmware.append(symbol);
    }
    setProperty(ResourcePropertyKey::kFirmware, fixedFirmware);
}

bool QnSecurityCamResource::trustCameraTime() const
{
    return QnLexical::deserialized<bool>(getProperty(ResourcePropertyKey::kTrustCameraTime));
}

void QnSecurityCamResource::setTrustCameraTime(bool value)
{
    setProperty(ResourcePropertyKey::kTrustCameraTime, boolToPropertyStr(value));
}

bool QnSecurityCamResource::keepCameraTimeSettings() const
{
    return QnLexical::deserialized<bool>(
        getProperty(ResourcePropertyKey::kKeepCameraTimeSettings), true);
}

void QnSecurityCamResource::setKeepCameraTimeSettings(bool value)
{
    setProperty(ResourcePropertyKey::kKeepCameraTimeSettings, boolToPropertyStr(value));
}

QString QnSecurityCamResource::getVendor() const
{
    SAFE(return m_vendor)

    // This code is commented for a reason. We want to know if vendor is empty.
    //SAFE(if (!m_vendor.isEmpty()) return m_vendor)    //calculated on the server
    //
    //QnResourceTypePtr resourceType = qnResTypePool->getResourceType(getTypeId());
    //return resourceType ? resourceType->getManufacturer() : QString(); //estimated value
}

void QnSecurityCamResource::setVendor(const QString& value)
{
    SAFE(m_vendor = value)
    emit vendorChanged(::toSharedPointer(this));
}

int QnSecurityCamResource::logicalId() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->logicalId(getIdForUserAttributes()).toInt();
}

void QnSecurityCamResource::setLogicalId(int value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());

    QString valueStr = value > 0 ? QString::number(value) : QString();
    if (userAttributesPool()->setLogicalId(getIdForUserAttributes(), valueStr))
        emit logicalIdChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setMaxPeriod(std::chrono::seconds value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    userAttributesPool()->setMaxPeriod(getIdForUserAttributes(), value);
}

std::chrono::seconds QnSecurityCamResource::maxPeriod() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->maxPeriod(getIdForUserAttributes());
}

void QnSecurityCamResource::setPreferredServerId(const QnUuid& value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    userAttributesPool()->setPreferredServerId(getIdForUserAttributes(), value);
}

QnUuid QnSecurityCamResource::preferredServerId() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->preferredServerId(getIdForUserAttributes());
}

void QnSecurityCamResource::synchronizeRemoteArchiveOnce()
{
    QString value;
    QnLexical::serialize(true, &value);
    setProperty(ResourcePropertyKey::kRemoteArchiveSynchronizationEnabledOnce, value);
}

void QnSecurityCamResource::setRemoteArchiveSynchronizationEnabled(bool isEnabled)
{
    QString value;
    QnLexical::serialize(isEnabled, &value);
    setProperty(ResourcePropertyKey::kRemoteArchiveSynchronizationEnabled, value);
}

bool QnSecurityCamResource::isRemoteArchiveSynchronizationEnabled() const
{
    bool isEnabled = false;
    QnLexical::deserialize(
        getProperty(ResourcePropertyKey::kRemoteArchiveSynchronizationEnabled),
        &isEnabled);

    return isEnabled;
}

void QnSecurityCamResource::updatePreferredServerId()
{
    if (preferredServerId().isNull())
        setPreferredServerId(getParentId());
}

void QnSecurityCamResource::setMinPeriod(std::chrono::seconds value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    userAttributesPool()->setMinPeriod(getIdForUserAttributes(), value);
}

std::chrono::seconds QnSecurityCamResource::minPeriod() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->minPeriod(getIdForUserAttributes());
}

int QnSecurityCamResource::recordBeforeMotionSec() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->recordBeforeMotionSec(getIdForUserAttributes());
}

void QnSecurityCamResource::setRecordBeforeMotionSec(int value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    userAttributesPool()->setRecordBeforeMotionSec(getIdForUserAttributes(), value);
}

int QnSecurityCamResource::recordAfterMotionSec() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->recordAfterMotionSec(getIdForUserAttributes());
}

void QnSecurityCamResource::setRecordAfterMotionSec(int value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    userAttributesPool()->setRecordAfterMotionSec(getIdForUserAttributes(), value);
}

void QnSecurityCamResource::setLicenseUsed(bool value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (userAttributesPool()->setLicenseUsed(getIdForUserAttributes(), value))
        emit licenseUsedChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::isLicenseUsed() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->licenseUsed(getIdForUserAttributes());
}

Qn::FailoverPriority QnSecurityCamResource::failoverPriority() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->failoverPriority(getIdForUserAttributes());
}

void QnSecurityCamResource::setFailoverPriority(Qn::FailoverPriority value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (userAttributesPool()->setFailoverPriority(getIdForUserAttributes(), value))
        emit failoverPriorityChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::isAudioRequired() const
{
    return m_cachedAudioRequired.get();
}

bool QnSecurityCamResource::isAudioForced() const
{
    return getProperty(ResourcePropertyKey::kForcedAudioStream).toInt() > 0;
}

bool QnSecurityCamResource::isAudioEnabled() const
{
    if (isAudioForced())
        return true;

    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (!userAttributesPool()->audioEnabled(getIdForUserAttributes()))
        return false;

    // If the camera has external audio input device but it does not exist anymore, use default value.
    if (!audioInputDeviceId().isNull() && !cameraExists(resourcePool(), audioInputDeviceId()))
        return false;

    return true;
}

void QnSecurityCamResource::setAudioEnabled(bool enabled)
{
    if (isAudioForced())
    {
        NX_ASSERT(false, "Enabled state of audio not changed for device with forced audio");
        return;
    }

    NX_ASSERT(!getIdForUserAttributes().isNull());

    const auto audioWasEnabled = isAudioEnabled();

    userAttributesPool()->setAudioEnabled(getIdForUserAttributes(), enabled);
    if (!audioInputDeviceId().isNull() && !cameraExists(resourcePool(), audioInputDeviceId()))
        setAudioInputDeviceId({});

    if (audioWasEnabled != isAudioEnabled())
        emit audioEnabledChanged(::toSharedPointer(this));
}

QnUuid QnSecurityCamResource::audioInputDeviceId() const
{
    return QnUuid::fromStringSafe(getProperty(ResourcePropertyKey::kAudioInputDeviceId));
}

void QnSecurityCamResource::setAudioInputDeviceId(const QnUuid& deviceId)
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
        // Null QString is always stored instead of string representation of null QnUuid to
        // prevent false property change notifications.
        setProperty(ResourcePropertyKey::kAudioInputDeviceId, QString());
        return;
    }

    setProperty(ResourcePropertyKey::kAudioInputDeviceId, deviceId.toString());
}

bool QnSecurityCamResource::isTwoWayAudioEnabled() const
{
    const auto enabled = QnLexical::deserialized(
        getProperty(ResourcePropertyKey::kTwoWayAudioEnabled), hasTwoWayAudio());

    // If has audio output device, but it is not exists any more use default value.
    auto audioOutputDeviceId = this->audioOutputDeviceId();
    return enabled
        && (audioOutputDeviceId.isNull() || cameraExists(resourcePool(), audioOutputDeviceId));
}

void QnSecurityCamResource::setTwoWayAudioEnabled(bool enabled)
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

QnUuid QnSecurityCamResource::audioOutputDeviceId() const
{
    return QnUuid::fromStringSafe(getProperty(ResourcePropertyKey::kAudioOutputDeviceId));
}

QnSecurityCamResourcePtr QnSecurityCamResource::audioOutputDevice() const
{
    const auto redirectedOutputCamera =
        resourcePool()->getResourceById<QnSecurityCamResource>(audioOutputDeviceId());

    if (redirectedOutputCamera)
        return redirectedOutputCamera;

    return toSharedPointer(this);
}

void QnSecurityCamResource::setAudioOutputDeviceId(const QnUuid& deviceId)
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
        // Null QString is always stored instead of string representation of null QnUuid to prevent
        // false property change notifications.
        setProperty(ResourcePropertyKey::kAudioOutputDeviceId, QString());
        return;
    }

    setProperty(ResourcePropertyKey::kAudioOutputDeviceId, deviceId.toString());
}

bool QnSecurityCamResource::isManuallyAdded() const
{
    return m_manuallyAdded;
}

void QnSecurityCamResource::setManuallyAdded(bool value)
{
    m_manuallyAdded = value;
}

bool QnSecurityCamResource::isDefaultAuth() const
{
    return hasCameraCapabilities(nx::vms::api::DeviceCapability::isDefaultPassword);
}

nx::vms::api::CameraBackupQuality QnSecurityCamResource::getBackupQuality() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->backupQuality(getIdForUserAttributes());
}

void QnSecurityCamResource::setBackupQuality(nx::vms::api::CameraBackupQuality value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (userAttributesPool()->setBackupQuality(getIdForUserAttributes(), value))
        emit backupQualitiesChanged(::toSharedPointer(this));
}

nx::vms::api::CameraBackupQuality QnSecurityCamResource::getActualBackupQualities() const
{
    nx::vms::api::CameraBackupQuality result = getBackupQuality();
    if (result != nx::vms::api::CameraBackupQuality::CameraBackup_Default)
        return result;

    /* If backup is not configured on this camera, use 'Backup newly added cameras' value */
    return commonModule()->globalSettings()->backupSettings().quality;
}

bool QnSecurityCamResource::isDualStreamingDisabled() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->disableDualStreaming(getIdForUserAttributes());
}

void QnSecurityCamResource::setDisableDualStreaming(bool value)
{

    NX_ASSERT(!getIdForUserAttributes().isNull());
    if (userAttributesPool()->setDisableDualStreaming(getIdForUserAttributes(), value))
    {
        m_cachedHasDualStreaming.reset();
        m_cachedMotionStreamIndex.reset();
        emit disableDualStreamingChanged(::toSharedPointer(this));
    }
}

bool QnSecurityCamResource::isPrimaryStreamRecorded() const
{
    const bool forbidden =
        getProperty(ResourcePropertyKey::kDontRecordPrimaryStreamKey).toInt() > 0;
    return !forbidden;
}

void QnSecurityCamResource::setPrimaryStreamRecorded(bool value)
{
    const QString valueStr = value ? "" : "1";
    setProperty(ResourcePropertyKey::kDontRecordPrimaryStreamKey, valueStr);
}

bool QnSecurityCamResource::isSecondaryStreamRecorded() const
{
    if (!hasDualStreamingInternal())
        return false;

    const bool forbidden =
        getProperty(ResourcePropertyKey::kDontRecordSecondaryStreamKey).toInt() > 0;
    return !forbidden;
}

void QnSecurityCamResource::setSecondaryStreamRecorded(bool value)
{
    if (!hasDualStreamingInternal())
        return;

    const QString valueStr = value ? "" : "1";
    setProperty(ResourcePropertyKey::kDontRecordSecondaryStreamKey, valueStr);
}

void QnSecurityCamResource::setCameraControlDisabled(bool value)
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    userAttributesPool()->setCameraControlDisabled(getIdForUserAttributes(), value);
}

bool QnSecurityCamResource::isCameraControlDisabled() const
{
    if (const auto commonModule = this->commonModule())
    {
        const auto& settings = commonModule->globalSettings();
        if (settings && !settings->isCameraSettingsOptimizationEnabled())
            return true;
    }

    return isCameraControlDisabledInternal();
}

bool QnSecurityCamResource::isCameraControlDisabledInternal() const
{
    NX_ASSERT(!getIdForUserAttributes().isNull());
    return userAttributesPool()->cameraControlDisabled(getIdForUserAttributes());
}

Qn::CameraStatusFlags QnSecurityCamResource::statusFlags() const
{
    SAFE(return m_statusFlags)
}

bool QnSecurityCamResource::hasStatusFlags(Qn::CameraStatusFlag value) const
{
    SAFE(return m_statusFlags & value)
}

void QnSecurityCamResource::setStatusFlags(Qn::CameraStatusFlags value)
{
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        if(m_statusFlags == value)
            return;
        m_statusFlags = value;
    }
    emit statusFlagsChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::addStatusFlags(Qn::CameraStatusFlag flag)
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

void QnSecurityCamResource::removeStatusFlags(Qn::CameraStatusFlag flag)
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

bool QnSecurityCamResource::needCheckIpConflicts() const {
    return !hasCameraCapabilities(nx::vms::api::DeviceCapability::shareIp);
}

QnTimePeriodList QnSecurityCamResource::getDtsTimePeriodsByMotionRegion(
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

bool QnSecurityCamResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source)
{
    QnSecurityCamResource* camera = dynamic_cast<QnSecurityCamResource*>(source.data());
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
                    (this->*setter)(newValue);
                    result = true;
                }
            }
        };

    // Group id and name can be changed for any resource because if we unable to authorize,
    // number of channels is not accessible.
    mergeValue(
        &QnSecurityCamResource::getGroupId,
        &QnSecurityCamResource::setGroupId,
        isStringEmpty);
    mergeValue(
        &QnSecurityCamResource::getDefaultGroupName,
        &QnSecurityCamResource::setDefaultGroupName,
        isStringEmpty);
    mergeValue(
        &QnSecurityCamResource::getModel,
        &QnSecurityCamResource::setModel,
        isStringEmpty);
    mergeValue(
        &QnSecurityCamResource::getVendor,
        &QnSecurityCamResource::setVendor,
        isStringEmpty);
    mergeValue(
        &QnSecurityCamResource::enforcedDeviceType,
        &QnSecurityCamResource::setDeviceType,
        isDeviceTypeEmpty);
    return result;
}

nx::vms::api::MotionTypes QnSecurityCamResource::calculateSupportedMotionTypes() const
{
    QString val = getProperty(ResourcePropertyKey::kSupportedMotion);
    if (val.isEmpty())
        return MotionType::software;

    MotionTypes result = MotionType::default_;
    for (const QString& str: val.split(','))
    {
        QString motionType = str.toLower().trimmed();
        if (motionType == "hardwaregrid")
            result |= MotionType::hardware;
        else if (motionType == "softwaregrid")
            result |= MotionType::software;
        else if (motionType == "motionwindow")
            result |= MotionType::window;
    }

    return result;
}

void QnSecurityCamResource::resetCachedValues()
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
    m_cachedHasVideo.reset();
    m_cachedMotionStreamIndex.reset();
}

bool QnSecurityCamResource::useBitratePerGop() const
{
    auto result = getProperty(ResourcePropertyKey::kBitratePerGOP);
    if (!result.isEmpty())
        return result.toInt() > 0;

    return resourceData().value<bool>(ResourceDataKey::kBitratePerGOP);
}

nx::core::resource::UsingOnvifMedia2Type QnSecurityCamResource::useMedia2ToFetchProfiles() const
{
    const auto usingMedia2Type = nx::reflect::fromString(
        getProperty(ResourcePropertyKey::kUseMedia2ToFetchProfiles).toStdString(),
        nx::core::resource::UsingOnvifMedia2Type::autoSelect);

    return usingMedia2Type;
}

bool QnSecurityCamResource::isIOModule() const
{
    return m_cachedIsIOModule.get();
}

nx::core::resource::AbstractRemoteArchiveManager* QnSecurityCamResource::remoteArchiveManager()
{
    return nullptr;
}

void QnSecurityCamResource::analyticsEventStarted(const QString& caption, const QString& description)
{
    emit analyticsEventStart(
        toSharedPointer(),
        caption,
        description,
        qnSyncTime->currentMSecsSinceEpoch());
}

void QnSecurityCamResource::analyticsEventEnded(const QString& caption, const QString& description)
{
    emit analyticsEventEnd(
        toSharedPointer(),
        caption,
        description,
        qnSyncTime->currentMSecsSinceEpoch());
}

float QnSecurityCamResource::rawSuggestBitrateKbps(
    Qn::StreamQuality quality, QSize resolution, int fps, const QString& codec)
{
    return nx::vms::common::CameraBitrateCalculator::suggestBitrateForQualityKbps(
        quality,
        resolution,
        fps,
        codec);
}

bool QnSecurityCamResource::captureEvent(const nx::vms::event::AbstractEventPtr& /*event*/)
{
    return false;
}

QString QnSecurityCamResource::vmsToAnalyticsEventTypeId(nx::vms::api::EventType /*eventType*/) const
{
    return QString();
}

Qn::ConnectionRole QnSecurityCamResource::toConnectionRole(StreamIndex index)
{
    return index == StreamIndex::primary
        ? Qn::CR_LiveVideo
        : Qn::CR_SecondaryLiveVideo;
}

nx::vms::api::StreamIndex QnSecurityCamResource::toStreamIndex(Qn::ConnectionRole role)
{
    return role == Qn::CR_SecondaryLiveVideo
        ? StreamIndex::secondary
        : StreamIndex::primary;
}

nx::core::ptz::PresetType QnSecurityCamResource::preferredPtzPresetType() const
{
    auto userPreference = userPreferredPtzPresetType();
    if (userPreference != nx::core::ptz::PresetType::undefined)
        return userPreference;

    return defaultPreferredPtzPresetType();
}

nx::core::ptz::PresetType QnSecurityCamResource::userPreferredPtzPresetType() const
{
    return nx::reflect::fromString(
        getProperty(ResourcePropertyKey::kUserPreferredPtzPresetType).toStdString(),
        nx::core::ptz::PresetType::undefined);
}

void QnSecurityCamResource::setUserPreferredPtzPresetType(nx::core::ptz::PresetType presetType)
{
    setProperty(ResourcePropertyKey::kUserPreferredPtzPresetType,
        presetType == nx::core::ptz::PresetType::undefined
            ? QString()
            : QString::fromStdString(nx::reflect::toString(presetType)));
}

nx::core::ptz::PresetType QnSecurityCamResource::defaultPreferredPtzPresetType() const
{
    return nx::reflect::fromString(
        getProperty(ResourcePropertyKey::kDefaultPreferredPtzPresetType).toStdString(),
        nx::core::ptz::PresetType::native);
}

void QnSecurityCamResource::setDefaultPreferredPtzPresetType(nx::core::ptz::PresetType presetType)
{
    setProperty(ResourcePropertyKey::kDefaultPreferredPtzPresetType,
        QString::fromStdString(nx::reflect::toString(presetType)));
}

Ptz::Capabilities QnSecurityCamResource::ptzCapabilitiesUserIsAllowedToModify() const
{
    return nx::reflect::fromString(
        getProperty(ResourcePropertyKey::kPtzCapabilitiesUserIsAllowedToModify).toStdString(),
        Ptz::Capabilities(Ptz::NoPtzCapabilities));
}

void QnSecurityCamResource::setPtzCapabilitesUserIsAllowedToModify(Ptz::Capabilities capabilities)
{
    setProperty(
        ResourcePropertyKey::kPtzCapabilitiesUserIsAllowedToModify,
        QString::fromStdString(nx::reflect::toString(capabilities)));
}

Ptz::Capabilities QnSecurityCamResource::ptzCapabilitiesAddedByUser() const
{
    return nx::reflect::fromString<Ptz::Capabilities>(
        getProperty(ResourcePropertyKey::kPtzCapabilitiesAddedByUser).toStdString(),
        Ptz::NoPtzCapabilities);
}

void QnSecurityCamResource::setPtzCapabilitiesAddedByUser(Ptz::Capabilities capabilities)
{
    setProperty(ResourcePropertyKey::kPtzCapabilitiesAddedByUser,
        QString::fromStdString(nx::reflect::toString(capabilities)));
}

QPointF QnSecurityCamResource::storedPtzPanTiltSensitivity() const
{
    return QJson::deserialized<QPointF>(
        getProperty(ResourcePropertyKey::kPtzPanTiltSensitivity).toLatin1(),
        QPointF(Ptz::kDefaultSensitivity, 0.0 /*uniformity flag*/));
}

QPointF QnSecurityCamResource::ptzPanTiltSensitivity() const
{
    const auto sensitivity = storedPtzPanTiltSensitivity();
    return sensitivity.y() > 0.0
        ? sensitivity
        : QPointF(sensitivity.x(), sensitivity.x());
}

bool QnSecurityCamResource::isPtzPanTiltSensitivityUniform() const
{
    return storedPtzPanTiltSensitivity().y() <= 0.0;
}

void QnSecurityCamResource::setPtzPanTiltSensitivity(const QPointF& value)
{
    setProperty(ResourcePropertyKey::kPtzPanTiltSensitivity, QString(QJson::serialized(QPointF(
        std::clamp(value.x(), Ptz::kMinimumSensitivity, Ptz::kMaximumSensitivity),
        value.y() > 0.0
            ? std::clamp(value.y(), Ptz::kMinimumSensitivity, Ptz::kMaximumSensitivity)
            : 0.0))));
}

void QnSecurityCamResource::setPtzPanTiltSensitivity(const qreal& uniformValue)
{
    setPtzPanTiltSensitivity(QPointF(uniformValue, 0.0 /*uniformity flag*/));
}

bool QnSecurityCamResource::isVideoQualityAdjustable() const
{
    return hasVideo()
        && !hasDefaultProperty(ResourcePropertyKey::kNoRecordingParams)
        && !hasCameraCapabilities(nx::vms::api::DeviceCapability::fixedQuality);
}

float QnSecurityCamResource::suggestBitrateKbps(
    const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const
{
    if (streamParams.bitrateKbps > 0)
    {
        auto result = streamParams.bitrateKbps;
        auto streamCapability = cameraMediaCapability().streamCapabilities.value(toStreamIndex(role));
        if (streamCapability.maxBitrateKbps > 0)
        {
            result = qBound(streamCapability.minBitrateKbps,
                result, streamCapability.maxBitrateKbps);
        }
        return result;
    }
    return suggestBitrateForQualityKbps(
        streamParams.quality, streamParams.resolution, streamParams.fps, streamParams.codec, role);
}

int QnSecurityCamResource::suggestBitrateForQualityKbps(Qn::StreamQuality quality,
    QSize resolution, int fps, const QString& codec, Qn::ConnectionRole role) const
{
    if (role == Qn::CR_Default)
        role = Qn::CR_LiveVideo;
    auto mediaCaps = cameraMediaCapability();
    auto streamCapability = mediaCaps.streamCapabilities.value(toStreamIndex(role));

    return nx::vms::common::CameraBitrateCalculator::suggestBitrateForQualityKbps(
        quality,
        resolution,
        fps,
        codec,
        streamCapability,
        useBitratePerGop());
}

bool QnSecurityCamResource::setCameraCredentialsSync(
    const QAuthenticator& /*auth*/, QString* outErrorString)
{
    if (outErrorString)
        *outErrorString = lit("Operation is not permitted.");
    return false;
}

nx::vms::common::MediaStreamEvent QnSecurityCamResource::checkForErrors() const
{
    const auto capabilities = getCameraCapabilities();
    if (capabilities.testFlag(nx::vms::api::DeviceCapability::isDefaultPassword))
        return nx::vms::common::MediaStreamEvent::forbiddenWithDefaultPassword;
    if (capabilities.testFlag(nx::vms::api::DeviceCapability::isOldFirmware))
        return nx::vms::common::MediaStreamEvent::oldFirmware;
    return nx::vms::common::MediaStreamEvent::noEvent;
}

QnResourceData QnSecurityCamResource::resourceData() const
{
    if (const auto commonModule = this->commonModule())
        return commonModule->resourceDataPool()->data(toSharedPointer(this));
    return {};
}

nx::vms::api::ExtendedCameraOutputs QnSecurityCamResource::extendedOutputs() const
{
    nx::vms::api::ExtendedCameraOutputs result{};
    for (const auto& port: ioPortDescriptions(Qn::IOPortType::PT_Output))
        result |= port.extendedCameraOutput();
    return result;
}

QnUuid QnSecurityCamResource::getIdForUserAttributes() const
{
    return getId();
}
