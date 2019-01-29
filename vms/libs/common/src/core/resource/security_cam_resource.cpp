#include "security_cam_resource.h"

#include <nx/utils/thread/mutex.h>

#include <api/global_settings.h>

#include <nx/fusion/serialization/lexical.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>

#include <common/common_module.h>

#include <recording/time_period_list.h>
#include "camera_user_attribute_pool.h"
#include "core/resource/media_server_resource.h"
#include "resource_data.h"
#include "api/model/api_ioport_data.h"
#include "nx/fusion/serialization/json.h"
#include <nx/fusion/model_functions.h>
#include "media_server_user_attributes.h"
#include <utils/common/synctime.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <utils/camera/camera_bitrate_calculator.h>

#define SAFE(expr) {QnMutexLocker lock( &m_mutex ); expr;}

using nx::vms::common::core::resource::CombinedSensorsDescription;

namespace {

static const int kDefaultMaxFps = 15;
static const int kShareFpsDefaultReservedSecondStreamFps = 2;
static const int kSharePixelsDefaultReservedSecondStreamFps = 0;
static const Qn::StreamFpsSharingMethod kDefaultStreamFpsSharingMethod = Qn::PixelsFpsSharing;
//static const Qn::MotionType defaultMotionType = Qn::MotionType::MT_MotionWindow;

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

} // namespace

const int QnSecurityCamResource::kDefaultSecondStreamFpsLow = 2;
const int QnSecurityCamResource::kDefaultSecondStreamFpsMedium = 7;
const int QnSecurityCamResource::kDefaultSecondStreamFpsHigh = 12;

QnUuid QnSecurityCamResource::makeCameraIdFromUniqueId(const QString& uniqueId)
{
    // ATTENTION: This logic is similar to the one in nx::vms::api::CameraData::fillId().
    if (uniqueId.isEmpty())
        return QnUuid();
    return guidFromArbitraryData(uniqueId);
}

void QnSecurityCamResource::setCommonModule(QnCommonModule* commonModule)
{
    base_type::setCommonModule(commonModule);
    connect(commonModule->dataPool(), &QnResourceDataPool::changed, this,
        &QnSecurityCamResource::resetCachedValues, Qt::DirectConnection);
}

QnSecurityCamResource::QnSecurityCamResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_recActionCnt(0),
    m_statusFlags(Qn::CameraStatusFlag::CSF_NoFlags),
    m_manuallyAdded(false),
    m_cachedLicenseType([this] { return calculateLicenseType(); }, &m_mutex),
    m_cachedHasDualStreaming(
        [this]()->bool{ return hasDualStreamingInternal() && !isDualStreamingDisabled(); },
        &m_mutex ),
    m_cachedSupportedMotionType(
        std::bind( &QnSecurityCamResource::calculateSupportedMotionType, this ),
        &m_mutex ),
    m_cachedCameraCapabilities(
        [this]()->Qn::CameraCapabilities {
            return static_cast<Qn::CameraCapabilities>(
                getProperty(ResourcePropertyKey::kCameraCapabilities).toInt()); },
        &m_mutex ),
    m_cachedIsDtsBased(
        [this]()->bool{ return getProperty(ResourcePropertyKey::kDts).toInt() > 0; },
        &m_mutex ),
    m_motionType(
        std::bind( &QnSecurityCamResource::calculateMotionType, this ),
        &m_mutex ),
    m_cachedIsIOModule(
        [this]()->bool{ return getProperty(ResourcePropertyKey::kIoConfigCapability).toInt() > 0; },
        &m_mutex),
    m_cachedCanConfigureRemoteRecording(
        [this]() { return getProperty(ResourcePropertyKey::kCanConfigureRemoteRecording).toInt() > 0; },
        &m_mutex),
    m_cachedCameraMediaCapabilities(
        [this]()
        {
            return QJson::deserialized<nx::media::CameraMediaCapability>(
                getProperty(ResourcePropertyKey::kMediaCapabilities).toUtf8());
        },
        &m_mutex),
    m_cachedDeviceType(
        [this]()
        {
            return QnLexical::deserialized<nx::core::resource::DeviceType>(
                getProperty(ResourcePropertyKey::kDeviceType),
                nx::core::resource::DeviceType::unknown);
        },
        &m_mutex),
    m_cachedHasVideo(
        [this]()
        {
            return !resourceData().value(ResourceDataKey::kNoVideoSupport, false);
        },
        &m_mutex),
    m_cachedMotionStreamIndex([this]{ return calculateMotionStreamIndex(); }, &m_mutex)
{
    addFlags(Qn::live_cam);

    connect(
        this, &QnResource::initializedChanged,
        this, &QnSecurityCamResource::at_initializedChanged,
        Qt::DirectConnection);
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

    connect(this, &QnNetworkResource::propertyChanged,
        [this](const QnResourcePtr& /*resource*/, const QString& key)
        {
            if (key == ResourcePropertyKey::kCameraCapabilities)
            {
                emit capabilitiesChanged(toSharedPointer());
            }
            else if (key == ResourcePropertyKey::kDts)
            {
                emit licenseTypeChanged(toSharedPointer(this));
            }
            else if (key == ResourcePropertyKey::kDeviceType)
            {
                emit licenseTypeChanged(toSharedPointer(this));
            }
            else if (key == ResourcePropertyKey::kUserPreferredPtzPresetType
                || key == ResourcePropertyKey::kDefaultPreferredPtzPresetType
                || key == ResourcePropertyKey::kPtzCapabilitiesAddedByUser
                || key == ResourcePropertyKey::kUserIsAllowedToOverridePtzCapabilities)
            {
                emit ptzConfigurationChanged(toSharedPointer(this));
            }
        });

    QnMediaResource::initMediaResource();
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
        && hasCameraCapabilities(Qn::SetUserPasswordCapability);
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

void QnSecurityCamResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    base_type::updateInternal(other, notifiers);

    QnSecurityCamResourcePtr other_casted = qSharedPointerDynamicCast<QnSecurityCamResource>(other);
    if (other_casted)
    {
        if (other_casted->m_groupId != m_groupId)
        {
            m_groupId = other_casted->m_groupId;
            notifiers << [r = toSharedPointer(this)]{emit r->groupIdChanged(r);};
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

int QnSecurityCamResource::getMaxFps() const
{
    const auto capabilities = cameraMediaCapability();
    int result = capabilities.streamCapabilities.value(Qn::StreamIndex::primary).maxFps;
    if (result > 0)
        return result;

    // Compatibility with version < 3.1.2
    QString value = getProperty(ResourcePropertyKey::kMaxFps);
    return value.isNull() ? kDefaultMaxFps : value.toInt();
}

void QnSecurityCamResource::setMaxFps(int fps)
{
    nx::media::CameraMediaCapability capability = cameraMediaCapability();
    capability.streamCapabilities[Qn::StreamIndex::primary].maxFps = fps;

    // We use direct setProperty() call instead of setCameraMediaCapability(),
    // because setMaxFps function should not save parameters.
    setProperty(ResourcePropertyKey::kMediaCapabilities,
        QString::fromLatin1(QJson::serialized(capability)));
}
int QnSecurityCamResource::reservedSecondStreamFps() const
{
    QString value = getProperty(lit("reservedSecondStreamFps"));

    if (!value.isNull())
    {
        bool ok = false;
        int reservedSecondStreamFps = value.toInt(&ok);

        if (ok)
            return reservedSecondStreamFps;

        NX_WARNING(this, lm("Wrong reserved second stream fps value for camera %1")
                .arg(getName()));
    }

    auto sharingMethod = streamFpsSharingMethod();

    if (sharingMethod == Qn::BasicFpsSharing)
        return kShareFpsDefaultReservedSecondStreamFps;
    else if (sharingMethod == Qn::PixelsFpsSharing)
        return kSharePixelsDefaultReservedSecondStreamFps;

    return 0;
}

bool QnSecurityCamResource::isEnoughFpsToRunSecondStream(int currentFps) const
{
    return streamFpsSharingMethod() != Qn::BasicFpsSharing || getMaxFps() - currentFps >= kDefaultSecondStreamFpsLow;
}

void QnSecurityCamResource::initializationDone()
{
    QnNetworkResource::initializationDone();
    resetCachedValues();
}

bool QnSecurityCamResource::hasVideo(const QnAbstractStreamDataProvider* dataProvider) const
{
    return m_cachedHasVideo.get();
}

Qn::LicenseType QnSecurityCamResource::calculateLicenseType() const
{
    if (isIOModule())
        return Qn::LC_IO;

    const QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId());

    if (resType && resType->getManufacture() == lit("VMAX"))
        return Qn::LC_VMAX;

    if (isDtsBased())
        return Qn::LC_Bridge;

    if (resType && resType->getManufacture() == lit("NetworkOptix"))
        return Qn::LC_Free;

    /**
     * AnalogEncoder should have priority over Analog type because of analog type is deprecated
     * (DW-CP04 has both analog and analogEncoder params)
     */
    if (isAnalogEncoder())
        return Qn::LC_AnalogEncoder;

    if (isAnalog())
        return Qn::LC_Analog;

    return Qn::LC_Professional;
}

bool QnSecurityCamResource::isRemoteArchiveMotionDetectionEnabled() const
{
    return QnMediaResource::edgeStreamValue()
        == getProperty(QnMediaResource::motionStreamKey()).toLower();
}

QList<QnMotionRegion> QnSecurityCamResource::getMotionRegionList() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock(userAttributesPool(), getId() );
    return (*userAttributesLock)->motionRegions;
}

QRegion QnSecurityCamResource::getMotionMask(int channel) const {
    return getMotionRegion(channel).getMotionMask();
}

QnSecurityCamResource::MotionStreamIndex QnSecurityCamResource::motionStreamIndex() const
{
    return m_cachedMotionStreamIndex.get();
}

QnMotionRegion QnSecurityCamResource::getMotionRegion(int channel) const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock(userAttributesPool(), getId() );
    const auto& regions = (*userAttributesLock)->motionRegions;
    if (regions.size() > channel)
        return regions[channel];
    else
        return QnMotionRegion();
}

void QnSecurityCamResource::setMotionRegion(const QnMotionRegion& mask, int channel)
{
    NX_ASSERT(!getId().isNull());
    Qn::MotionType motionType = Qn::MotionType::MT_Default;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
        auto& regions = (*userAttributesLock)->motionRegions;
        while (channel >= regions.size())
            regions << QnMotionRegion();
        if (regions[channel] == mask)
            return;
        regions[channel] = mask;
        motionType = (*userAttributesLock)->motionType;
    }

    if (motionType != Qn::MotionType::MT_SoftwareGrid)
        setMotionMaskPhysical(channel);

    emit motionRegionChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setMotionRegionList(const QList<QnMotionRegion>& maskList)
{
    NX_ASSERT(!getId().isNull());
    Qn::MotionType motionType = Qn::MotionType::MT_Default;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );

        if( (*userAttributesLock)->motionRegions == maskList )
            return;
        (*userAttributesLock)->motionRegions = maskList;
        motionType = (*userAttributesLock)->motionType;
    }

    if (motionType != Qn::MotionType::MT_SoftwareGrid)
    {
        for (int i = 0; i < getVideoLayout()->channelCount(); ++i)
            setMotionMaskPhysical(i);
    }

    emit motionRegionChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setScheduleTasks(const QnScheduleTaskList& scheduleTasks)
{
    NX_ASSERT(!getId().isNull());
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
        if ((*userAttributesLock)->scheduleTasks == scheduleTasks)
            return;
        (*userAttributesLock)->scheduleTasks = scheduleTasks;
    }
    emit scheduleTasksChanged(::toSharedPointer(this));
}

QnScheduleTaskList QnSecurityCamResource::getScheduleTasks() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->scheduleTasks;
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
    saveProperties();
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
    return m_cachedIsDtsBased.get() || deviceType() == nx::core::resource::DeviceType::nvr;
}

bool QnSecurityCamResource::canConfigureRecording() const
{
    return !m_cachedIsDtsBased.get() || m_cachedCanConfigureRemoteRecording.get();
}

bool QnSecurityCamResource::isAnalog() const
{
    QString val = getProperty(ResourcePropertyKey::kAnalog);
    return val.toInt() > 0;
}

bool QnSecurityCamResource::isAnalogEncoder() const
{
    if (deviceType() == nx::core::resource::DeviceType::encoder)
        return true;

    return resourceData().value<bool>(lit("analogEncoder"));
}

CombinedSensorsDescription QnSecurityCamResource::combinedSensorsDescription() const
{
    const auto& value = getProperty(ResourcePropertyKey::kCombinedSensorsDescription);
    return QJson::deserialized<CombinedSensorsDescription>(value.toLatin1());
}

void QnSecurityCamResource::setCombinedSensorsDescription(
    const CombinedSensorsDescription& sensorsDescription)
{
    setProperty(ResourcePropertyKey::kCombinedSensorsDescription,
        QString::fromLatin1(QJson::serialized(sensorsDescription)));
}

bool QnSecurityCamResource::hasCombinedSensors() const
{
    return !combinedSensorsDescription().isEmpty();
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
        && !nx::core::resource::isProxyDeviceType(deviceType());
}

nx::core::resource::DeviceType QnSecurityCamResource::deviceType() const
{
    return m_cachedDeviceType.get();
}

void QnSecurityCamResource::setDeviceType(nx::core::resource::DeviceType deviceType)
{
    m_cachedDeviceType.reset();
    m_cachedLicenseType.reset();
    setProperty(ResourcePropertyKey::kDeviceType, QnLexical::serialized(deviceType));
}

Qn::LicenseType QnSecurityCamResource::licenseType() const
{
    return m_cachedLicenseType.get();
}

Qn::StreamFpsSharingMethod QnSecurityCamResource::streamFpsSharingMethod() const {
    QString sval = getProperty(ResourcePropertyKey::kStreamFpsSharing);
    if (sval.isEmpty())
        return kDefaultStreamFpsSharingMethod;

    if (sval == lit("shareFps"))
        return Qn::BasicFpsSharing;
    if (sval == lit("noSharing"))
        return Qn::NoFpsSharing;
    return Qn::PixelsFpsSharing;
}

void QnSecurityCamResource::setStreamFpsSharingMethod(Qn::StreamFpsSharingMethod value)
{
    switch( value )
    {
        case Qn::BasicFpsSharing:
            setProperty(ResourcePropertyKey::kStreamFpsSharing, lit("shareFps"));
            break;
        case Qn::NoFpsSharing:
            setProperty(ResourcePropertyKey::kStreamFpsSharing, lit("noSharing"));
            break;
        default:
            setProperty(ResourcePropertyKey::kStreamFpsSharing, lit("sharePixels"));
            break;
    }
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
        hasInputs |= supportedTypes & Qn::PT_Input;
        hasOutputs |= supportedTypes & Qn::PT_Output;
    }

    setProperty(ResourcePropertyKey::kIoSettings, QString::fromUtf8(QJson::serialized(newPorts)));
    setCameraCapability(Qn::InputPortCapability, hasInputs);
    setCameraCapability(Qn::OutputPortCapability, hasOutputs);
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

void QnSecurityCamResource::at_initializedChanged()
{
    emit licenseTypeChanged(toSharedPointer());
}

void QnSecurityCamResource::at_motionRegionChanged()
{
    if (flags() & Qn::foreigner)
        return;

    if (getMotionType() == Qn::MotionType::MT_HardwareGrid
        || getMotionType() == Qn::MotionType::MT_MotionWindow)
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
    return getCameraCapabilities().testFlag(Qn::AudioTransmitCapability);
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

Qn::MotionType QnSecurityCamResource::getDefaultMotionType() const
{
    Qn::MotionTypes value = supportedMotionType();
    if (value.testFlag(Qn::MotionType::MT_SoftwareGrid))
        return Qn::MotionType::MT_SoftwareGrid;

    if (value.testFlag(Qn::MotionType::MT_HardwareGrid))
        return Qn::MotionType::MT_HardwareGrid;

    if (value.testFlag(Qn::MotionType::MT_MotionWindow))
        return Qn::MotionType::MT_MotionWindow;

    return Qn::MotionType::MT_NoMotion;
}

Qn::MotionTypes QnSecurityCamResource::supportedMotionType() const
{
    return m_cachedSupportedMotionType.get();
}

bool QnSecurityCamResource::hasMotion() const
{
    Qn::MotionType motionType = getDefaultMotionType();
    if (motionType == Qn::MotionType::MT_SoftwareGrid)
    {
        return hasDualStreaming()
            || (getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability)
            || !getProperty(QnMediaResource::motionStreamKey()).isEmpty();
    }
    return motionType != Qn::MotionType::MT_NoMotion;
}

Qn::MotionType QnSecurityCamResource::getMotionType() const
{
    return m_motionType.get();
}

Qn::MotionType QnSecurityCamResource::calculateMotionType() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    Qn::MotionType value = (*userAttributesLock)->motionType;
    if (value == Qn::MotionType::MT_NoMotion)
        return value;

    if (value == Qn::MotionType::MT_Default || !(supportedMotionType() & value))
        return getDefaultMotionType();

    return value;
}

QnSecurityCamResource::MotionStreamIndex QnSecurityCamResource::calculateMotionStreamIndex() const
{
    const auto forcedMotionStreamStr = getProperty(motionStreamKey()).toLower();
    if (!forcedMotionStreamStr.isEmpty())
    {
        const auto forcedMotionStream = QnLexical::deserialized<nx::vms::api::MotionStreamType>(
            forcedMotionStreamStr, nx::vms::api::MotionStreamType::automatic);

        switch (forcedMotionStream)
        {
            case nx::vms::api::MotionStreamType::primary:
                return {Qn::StreamIndex::primary, /*isForced*/ true};
            case nx::vms::api::MotionStreamType::secondary:
                return {Qn::StreamIndex::secondary, /*isForced*/ true};
            case nx::vms::api::MotionStreamType::edge:
                NX_ASSERT(false, "This value was not handled and is used only in isRemoteArchiveMotionDetectionEnabled()");
                break;
            default:
                NX_ASSERT(false, "Automatic stream type should not be forced");
                break;
        }
    }

    if (!hasDualStreaming()
        && getCameraCapabilities().testFlag(Qn::PrimaryStreamSoftMotionCapability))
    {
        return {Qn::StreamIndex::primary, /*isForced*/ false};
    }

    return {Qn::StreamIndex::secondary, /*isForced*/ false};
}

void QnSecurityCamResource::setMotionType(Qn::MotionType value)
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    (*userAttributesLock)->motionType = value;
}

Qn::CameraCapabilities QnSecurityCamResource::getCameraCapabilities() const
{
    return m_cachedCameraCapabilities.get();
}

bool QnSecurityCamResource::hasCameraCapabilities(Qn::CameraCapabilities capabilities) const
{
    return (getCameraCapabilities() & capabilities) == capabilities;
}

void QnSecurityCamResource::setCameraCapabilities(Qn::CameraCapabilities capabilities)
{
    setProperty(ResourcePropertyKey::kCameraCapabilities, static_cast<int>(capabilities));
    m_cachedCameraCapabilities.reset();
}

void QnSecurityCamResource::setCameraCapability(Qn::CameraCapability capability, bool value)
{
    setCameraCapabilities(value ? (getCameraCapabilities() | capability) : (getCameraCapabilities() & ~capability));
}

bool QnSecurityCamResource::isRecordingEventAttached() const
{
    return m_recActionCnt > 0;
}

void QnSecurityCamResource::recordingEventAttached()
{
    m_recActionCnt++;
}

void QnSecurityCamResource::recordingEventDetached()
{
    m_recActionCnt = qMax(0, m_recActionCnt-1);
}

QString QnSecurityCamResource::getUserDefinedName() const
{
    if (!getId().isNull() && commonModule())
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
        if( !(*userAttributesLock)->name.isEmpty() )
            return (*userAttributesLock)->name;
    }

    return QnSecurityCamResource::getName();
}

QString QnSecurityCamResource::getUserDefinedGroupName() const
{
    if (!getId().isNull() && commonModule())
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(
            userAttributesPool(),
            getId() );
        if( !(*userAttributesLock)->groupName.isEmpty() )
            return (*userAttributesLock)->groupName;
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
        QnMutexLocker locker( &m_mutex );
        if(m_groupName == value)
            return;
        m_groupName = value;
    }
    emit groupNameChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setUserDefinedGroupName( const QString& value )
{
    NX_ASSERT(!getId().isNull());
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(
            userAttributesPool(),
            getId() );
        if( (*userAttributesLock)->groupName == value )
            return;
        (*userAttributesLock)->groupName = value;
    }
    emit groupNameChanged(::toSharedPointer(this));
}

QString QnSecurityCamResource::getGroupId() const
{
    SAFE(return m_groupId)
}

void QnSecurityCamResource::setGroupId(const QString& value)
{
    {
        QnMutexLocker locker( &m_mutex );
        if(m_groupId == value)
            return;
        m_groupId = value;
    }
    emit groupIdChanged(::toSharedPointer(this));

}

QString QnSecurityCamResource::getSharedId() const
{
    {
        QnMutexLocker lock(&m_mutex);
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
    QnMutexLocker lk(&m_mutex);
    m_model = model;
}

QString QnSecurityCamResource::getFirmware() const
{
    return getProperty(ResourcePropertyKey::kFirmware);
}

void QnSecurityCamResource::setFirmware(const QString &firmware)
{
    setProperty(ResourcePropertyKey::kFirmware, firmware);
}

bool QnSecurityCamResource::trustCameraTime() const
{
    return QnLexical::deserialized<bool>(getProperty(ResourcePropertyKey::kTrustCameraTime));
}

void QnSecurityCamResource::setTrustCameraTime(bool value)
{
    setProperty(ResourcePropertyKey::kTrustCameraTime, boolToPropertyStr(value));
}

QString QnSecurityCamResource::getVendor() const
{
    SAFE(return m_vendor)

    // This code is commented for a reason. We want to know if vendor is empty. --Elric
    //SAFE(if (!m_vendor.isEmpty()) return m_vendor)    //calculated on the server
    //
    //QnResourceTypePtr resourceType = qnResTypePool->getResourceType(getTypeId());
    //return resourceType ? resourceType->getManufacture() : QString(); //estimated value
}

void QnSecurityCamResource::setVendor(const QString& value)
{
    SAFE(m_vendor = value)
}

int QnSecurityCamResource::logicalId() const
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock(userAttributesPool(), getId());
    return (*userAttributesLock)->logicalId.toInt();
}

void QnSecurityCamResource::setLogicalId(int value)
{
    NX_ASSERT(!getId().isNull());
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(userAttributesPool(), getId());
        if ((*userAttributesLock)->logicalId.toInt() == value)
            return;
        (*userAttributesLock)->logicalId = value > 0 ? QString::number(value) : QString();
    }

    emit logicalIdChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setMaxDays(int value)
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    (*userAttributesLock)->maxDays = value;
}

int QnSecurityCamResource::maxDays() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->maxDays;
}

void QnSecurityCamResource::setPreferredServerId(const QnUuid& value)
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    (*userAttributesLock)->preferredServerId = value;
}

QnUuid QnSecurityCamResource::preferredServerId() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->preferredServerId;
}

void QnSecurityCamResource::setMinDays(int value)
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    (*userAttributesLock)->minDays = value;
}

int QnSecurityCamResource::minDays() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->minDays;
}

int QnSecurityCamResource::recordBeforeMotionSec() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->recordBeforeMotionSec;
}

void QnSecurityCamResource::setRecordBeforeMotionSec(int value)
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    (*userAttributesLock)->recordBeforeMotionSec = value;
}

int QnSecurityCamResource::recordAfterMotionSec() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->recordAfterMotionSec;
}

void QnSecurityCamResource::setRecordAfterMotionSec(int value)
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    (*userAttributesLock)->recordAfterMotionSec = value;
}

void QnSecurityCamResource::setLicenseUsed(bool value)
{
    NX_ASSERT(!getId().isNull());
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
        if ((*userAttributesLock)->licenseUsed == value)
            return;
        (*userAttributesLock)->licenseUsed = value;
    }

    emit licenseUsedChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::isLicenseUsed() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->licenseUsed;
}

Qn::FailoverPriority QnSecurityCamResource::failoverPriority() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->failoverPriority;
}

void QnSecurityCamResource::setFailoverPriority(Qn::FailoverPriority value)
{
    NX_ASSERT(!getId().isNull());
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
        if ((*userAttributesLock)->failoverPriority == value)
            return;
        (*userAttributesLock)->failoverPriority = value;
    }

    emit failoverPriorityChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setAudioEnabled(bool enabled)
{
    NX_ASSERT(!getId().isNull());
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(userAttributesPool(), getId());
        if ((*userAttributesLock)->audioEnabled == enabled)
            return;
        (*userAttributesLock)->audioEnabled = enabled;
    }

    emit audioEnabledChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::isAudioForced() const
{
    return getProperty(ResourcePropertyKey::kForcedAudioStream).toInt() > 0;
}

bool QnSecurityCamResource::isAudioEnabled() const
{
    if (isAudioForced())
        return true;

    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->audioEnabled;
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
    return hasCameraCapabilities(Qn::IsDefaultPasswordCapability);
}

Qn::CameraBackupQualities QnSecurityCamResource::getBackupQualities() const
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->backupQualities;
}

void QnSecurityCamResource::setBackupQualities(Qn::CameraBackupQualities value)
{
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
        if ((*userAttributesLock)->backupQualities == value)
            return;
        (*userAttributesLock)->backupQualities = value;
    }
    emit backupQualitiesChanged(::toSharedPointer(this));
}

Qn::CameraBackupQualities QnSecurityCamResource::getActualBackupQualities() const
{
    Qn::CameraBackupQualities result = getBackupQualities();

    if (result == Qn::CameraBackupQuality::CameraBackup_Disabled)
        return result;

    const auto& settings = commonModule()->globalSettings();

    auto value = settings->backupQualities();

    /* If backup is not configured on this camera, use 'Backup newly added cameras' value */
    if (result == Qn::CameraBackupQuality::CameraBackup_Default)
    {
        return settings->backupNewCamerasByDefault()
            ? value
            : Qn::CameraBackupQuality::CameraBackup_Disabled;
    }

    return value;
}

void QnSecurityCamResource::setDisableDualStreaming(bool value)
{
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock(userAttributesPool(), getId());
        if ((*userAttributesLock)->disableDualStreaming == value)
            return;
        (*userAttributesLock)->disableDualStreaming = value;
    }

    m_cachedHasDualStreaming.reset();
    emit disableDualStreamingChanged(toSharedPointer());
}

bool QnSecurityCamResource::isDualStreamingDisabled() const
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->disableDualStreaming;
}

void QnSecurityCamResource::setCameraControlDisabled(bool value)
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    (*userAttributesLock)->cameraControlDisabled = value;
}

bool QnSecurityCamResource::isCameraControlDisabled() const
{
    const auto& settings = commonModule()->globalSettings();

    if (settings && !settings->isCameraSettingsOptimizationEnabled())
        return true;
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->cameraControlDisabled;
}

int QnSecurityCamResource::defaultSecondaryFps(Qn::StreamQuality quality) const
{
    switch (quality)
    {
        case Qn::StreamQuality::lowest:
        case Qn::StreamQuality::low:
            return kDefaultSecondStreamFpsLow;
        case Qn::StreamQuality::normal:
            return kDefaultSecondStreamFpsMedium;
        case Qn::StreamQuality::high:
        case Qn::StreamQuality::highest:
            return kDefaultSecondStreamFpsHigh;
        default:
            break;
    }
    return kDefaultSecondStreamFpsMedium;
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
        QnMutexLocker locker( &m_mutex );
        if(m_statusFlags == value)
            return;
        m_statusFlags = value;
    }
    emit statusFlagsChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::addStatusFlags(Qn::CameraStatusFlag flag)
{
    {
        QnMutexLocker locker( &m_mutex );
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
        QnMutexLocker locker( &m_mutex );
        Qn::CameraStatusFlags value = m_statusFlags & ~flag;
        if(m_statusFlags == value)
            return;
        m_statusFlags = value;
    }
    emit statusFlagsChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::needCheckIpConflicts() const {
    return !hasCameraCapabilities(Qn::ShareIpCapability);
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
        &QnSecurityCamResource::deviceType,
        &QnSecurityCamResource::setDeviceType,
        isDeviceTypeEmpty);
    return result;
}

//QnMotionRegion QnSecurityCamResource::getMotionRegionNonSafe(int channel) const
//{
//    return m_motionMaskList[channel];
//}

Qn::MotionTypes QnSecurityCamResource::calculateSupportedMotionType() const
{
    QString val = getProperty(ResourcePropertyKey::kSupportedMotion);
    if (val.isEmpty())
        return Qn::MotionType::MT_SoftwareGrid;

    Qn::MotionTypes result = Qn::MotionType::MT_Default;
    for(const QString& str: val.split(L','))
    {
        QString s1 = str.toLower().trimmed();
        if (s1 == lit("hardwaregrid"))
            result |= Qn::MotionType::MT_HardwareGrid;
        else if (s1 == lit("softwaregrid"))
            result |= Qn::MotionType::MT_SoftwareGrid;
        else if (s1 == lit("motionwindow"))
            result |= Qn::MotionType::MT_MotionWindow;
    }

    return result;
}

void QnSecurityCamResource::resetCachedValues()
{
    // TODO: #rvasilenko reset only required values on property changed (as in server resource).
    //resetting cached values
    m_cachedHasDualStreaming.reset();
    m_cachedSupportedMotionType.reset();
    m_cachedCameraCapabilities.reset();
    m_cachedIsDtsBased.reset();
    m_motionType.reset();
    m_cachedIsIOModule.reset();
    m_cachedCanConfigureRemoteRecording.reset();
    m_cachedCameraMediaCapabilities.reset();
    m_cachedLicenseType.reset();
    m_cachedDeviceType.reset();
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

float QnSecurityCamResource::rawSuggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps)
{
    return nx::core::CameraBitrateCalculator::suggestBitrateForQualityKbps(
        quality,
        resolution,
        fps);
}

bool QnSecurityCamResource::captureEvent(const nx::vms::event::AbstractEventPtr& event)
{
    return false;
}

bool QnSecurityCamResource::isAnalyticsDriverEvent(nx::vms::api::EventType eventType) const
{
    return eventType == nx::vms::api::EventType::analyticsSdkEvent;
}

Qn::StreamIndex QnSecurityCamResource::toStreamIndex(Qn::ConnectionRole role)
{
    return role == Qn::CR_SecondaryLiveVideo ? Qn::StreamIndex::secondary : Qn::StreamIndex::primary;
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
    return QnLexical::deserialized(
        getProperty(ResourcePropertyKey::kUserPreferredPtzPresetType),
        nx::core::ptz::PresetType::undefined);
}

void QnSecurityCamResource::setUserPreferredPtzPresetType(nx::core::ptz::PresetType presetType)
{
    setProperty(ResourcePropertyKey::kUserPreferredPtzPresetType, QnLexical::serialized(presetType));
}

nx::core::ptz::PresetType QnSecurityCamResource::defaultPreferredPtzPresetType() const
{
    return QnLexical::deserialized(
        getProperty(ResourcePropertyKey::kDefaultPreferredPtzPresetType),
        nx::core::ptz::PresetType::native);
}

void QnSecurityCamResource::setDefaultPreferredPtzPresetType(nx::core::ptz::PresetType presetType)
{
    setProperty(ResourcePropertyKey::kDefaultPreferredPtzPresetType, QnLexical::serialized(presetType));
}

bool QnSecurityCamResource::isUserAllowedToModifyPtzCapabilities() const
{
    return QnLexical::deserialized(getProperty(ResourcePropertyKey::kUserIsAllowedToOverridePtzCapabilities), false);
}

void QnSecurityCamResource::setIsUserAllowedToModifyPtzCapabilities(bool allowed)
{
    setProperty(
        ResourcePropertyKey::kUserIsAllowedToOverridePtzCapabilities,
        QnLexical::serialized(allowed));
}

Ptz::Capabilities QnSecurityCamResource::ptzCapabilitiesAddedByUser() const
{
    return QnLexical::deserialized<Ptz::Capabilities>(
        getProperty(ResourcePropertyKey::kPtzCapabilitiesAddedByUser), Ptz::NoPtzCapabilities);
}

void QnSecurityCamResource::setPtzCapabilitiesAddedByUser(Ptz::Capabilities capabilities)
{
    setProperty(ResourcePropertyKey::kPtzCapabilitiesAddedByUser, QnLexical::serialized(capabilities));
}

int QnSecurityCamResource::suggestBitrateKbps(const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const
{
    if (streamParams.bitrateKbps > 0)
    {
        auto result = streamParams.bitrateKbps;
        auto streamCapability = cameraMediaCapability().streamCapabilities.value(toStreamIndex(role));
        if (streamCapability.maxBitrateKbps > 0)
            result = qBound(streamCapability.minBitrateKbps, result, streamCapability.maxBitrateKbps);
        return result;
    }
    return suggestBitrateForQualityKbps(streamParams.quality, streamParams.resolution, streamParams.fps, role);
}

int QnSecurityCamResource::suggestBitrateForQualityKbps(Qn::StreamQuality quality, QSize resolution, int fps, Qn::ConnectionRole role) const
{
    if (role == Qn::CR_Default)
        role = Qn::CR_LiveVideo;
    auto mediaCaps = cameraMediaCapability();
    auto streamCapability = mediaCaps.streamCapabilities.value(toStreamIndex(role));

    return nx::core::CameraBitrateCalculator::suggestBitrateForQualityKbps(
        quality,
        resolution,
        fps,
        streamCapability,
        useBitratePerGop());
}

bool QnSecurityCamResource::setCameraCredentialsSync(
    const QAuthenticator& auth, QString* outErrorString)
{
    if (outErrorString)
        *outErrorString = lit("Operation is not permitted.");
    return false;
}

Qn::MediaStreamEvent QnSecurityCamResource::checkForErrors() const
{
    const auto capabilities = getCameraCapabilities();
    if (capabilities.testFlag(Qn::IsDefaultPasswordCapability))
        return Qn::MediaStreamEvent::ForbiddenWithDefaultPassword;
    if (capabilities.testFlag(Qn::IsOldFirmwareCapability))
        return Qn::MediaStreamEvent::oldFirmware;
    return Qn::MediaStreamEvent::NoEvent;
}

QnResourceData QnSecurityCamResource::resourceData() const
{
    return commonModule()->dataPool()->data(toSharedPointer(this));
}
