#include "security_cam_resource.h"

#include <nx/utils/thread/mutex.h>

#include <api/global_settings.h>

#include <nx/fusion/serialization/lexical.h>

#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>

#include <core/dataprovider/data_provider_factory.h>

#include "user_resource.h"
#include "common/common_module.h"

#include <recording/time_period_list.h>
#include "camera_user_attribute_pool.h"
#include "core/resource/media_server_resource.h"
#include "resource_data.h"
#include "api/model/api_ioport_data.h"
#include "nx/fusion/serialization/json.h"
#include <nx/fusion/model_functions.h>
#include "media_server_user_attributes.h"
#include <common/static_common_module.h>
#include <utils/common/synctime.h>

#include <nx/utils/log/log.h>

#define SAFE(expr) {QnMutexLocker lock( &m_mutex ); expr;}

using nx::vms::common::core::resource::CombinedSensorsDescription;

namespace {

static const int kDefaultMaxFps = 15;
static const int kShareFpsDefaultReservedSecondStreamFps = 2;
static const int kSharePixelsDefaultReservedSecondStreamFps = 0;
static const Qn::StreamFpsSharingMethod kDefaultStreamFpsSharingMethod = Qn::PixelsFpsSharing;
//static const Qn::MotionType defaultMotionType = Qn::MT_MotionWindow;

} // namespace

const int QnSecurityCamResource::kDefaultSecondStreamFpsLow = 2;
const int QnSecurityCamResource::kDefaultSecondStreamFpsMedium = 7;
const int QnSecurityCamResource::kDefaultSecondStreamFpsHigh = 12;

QnUuid QnSecurityCamResource::makeCameraIdFromUniqueId(const QString& uniqueId)
{
    // ATTENTION: This logic is similar to the one in ApiCameraData::fillId().
    if (uniqueId.isEmpty())
        return QnUuid();
    return guidFromArbitraryData(uniqueId);
}

//const int PRIMARY_ENCODER_INDEX = 0;
//const int SECONDARY_ENCODER_INDEX = 1;

QnSecurityCamResource::QnSecurityCamResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_dpFactory(0),
    m_recActionCnt(0),
    m_statusFlags(Qn::CSF_NoFlags),
    m_manuallyAdded(false),
    m_cachedLicenseType([this] { return calculateLicenseType(); }, &m_mutex),
    m_cachedHasDualStreaming2(
        [this]()->bool{ return hasDualStreaming() && !isDualStreamingDisabled(); },
        &m_mutex ),
    m_cachedSupportedMotionType(
        std::bind( &QnSecurityCamResource::calculateSupportedMotionType, this ),
        &m_mutex ),
    m_cachedCameraCapabilities(
        [this]()->Qn::CameraCapabilities {
            return static_cast<Qn::CameraCapabilities>(
                getProperty(Qn::CAMERA_CAPABILITIES_PARAM_NAME).toInt()); },
        &m_mutex ),
    m_cachedIsDtsBased(
        [this]()->bool{ return getProperty(Qn::DTS_PARAM_NAME).toInt() > 0; },
        &m_mutex ),
    m_motionType(
        std::bind( &QnSecurityCamResource::calculateMotionType, this ),
        &m_mutex ),
    m_cachedIsIOModule(
        [this]()->bool{ return getProperty(Qn::IO_CONFIG_PARAM_NAME).toInt() > 0; },
        &m_mutex),
    m_cachedAnalyticsSupportedEvents(
        [this]()
        {
            return QJson::deserialized<nx::api::AnalyticsSupportedEvents>(
                getProperty(Qn::kAnalyticsDriversParamName).toUtf8());
        },
        &m_mutex),
    m_cachedCameraMediaCapabilities(
        [this]()
        {
            return QJson::deserialized<nx::media::CameraMediaCapability>(
                getProperty(nx::media::kCameraMediaCapabilityParamName).toUtf8());
        },
        &m_mutex)
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
        this, &QnSecurityCamResource::motionRegionChanged,
        this, &QnSecurityCamResource::at_motionRegionChanged,
        Qt::DirectConnection);

    connect(this, &QnNetworkResource::propertyChanged, this,
        [this](const QnResourcePtr& /*resource*/, const QString& key)
        {
            if (key == Qn::CAMERA_CAPABILITIES_PARAM_NAME)
                emit capabilitiesChanged(toSharedPointer());
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

bool QnSecurityCamResource::removeProperty(const QString& key)
{
	return QnResource::removeProperty(key);
}

bool QnSecurityCamResource::isGroupPlayOnly() const {
    return hasParam(Qn::kGroupPlayParamName);
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
    if (!capabilities.isNull())
        return capabilities.streamCapabilities.value(Qn::CR_LiveVideo).maxFps;

    // Compatibility with version < 3.1.2
    QString value = getProperty(Qn::MAX_FPS_PARAM_NAME);
    return value.isNull() ? kDefaultMaxFps : value.toInt();
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

        NX_LOGX(
            lm("Wrong reserved second stream fps value for camera %1")
                .arg(getName()),
            cl_logWARNING);
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

#ifdef ENABLE_DATA_PROVIDERS
QnAbstractStreamDataProvider* QnSecurityCamResource::createDataProviderInternal(Qn::ConnectionRole role)
{
    if (role == Qn::CR_SecondaryLiveVideo && !hasDualStreaming2())
        return nullptr;

    switch (role)
    {
        case Qn::CR_SecondaryLiveVideo:
        case Qn::CR_Default:
        case Qn::CR_LiveVideo:
        {
            QnAbstractStreamDataProvider* result = createLiveDataProvider();
            if (result)
                result->setRole(role);
            return result;
        }
        case Qn::CR_Archive:
        {
            if (QnAbstractStreamDataProvider* result = createArchiveDataProvider())
                return result;

            /* This is the only legal break. */
            break;
        }
        default:
            NX_ASSERT(false, "There are no other roles");
            break;
    }

    NX_ASSERT(role == Qn::CR_Archive);
    /* The one and only QnDataProviderFactory now is the QnServerDataProviderFactory class
     * which handles only Qn::CR_Archive role. */
    if (m_dpFactory)
        return m_dpFactory->createDataProviderInternal(toSharedPointer(), role);

    return nullptr;
}
#endif // ENABLE_DATA_PROVIDERS

void QnSecurityCamResource::initializationDone()
{
    //m_initMutex is locked down the stack
    QnNetworkResource::initializationDone();
    if( m_inputPortListenerCount.load() > 0 )
        startInputPortMonitoringAsync( std::function<void(bool)>() );
    resetCachedValues();
}

bool QnSecurityCamResource::startInputPortMonitoringAsync( std::function<void(bool)>&& /*completionHandler*/ )
{
    return false;
}

void QnSecurityCamResource::stopInputPortMonitoringAsync()
{
}

bool QnSecurityCamResource::isInputPortMonitored() const {
    return false;
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

    if (isEdge())
        return Qn::LC_Edge;

    return Qn::LC_Professional;
}

bool QnSecurityCamResource::isRemoteArchiveMotionDetectionEnabled() const
{
    return QnMediaResource::edgeStreamValue()
        == getProperty(QnMediaResource::motionStreamKey()).toLower();
}

void QnSecurityCamResource::setDataProviderFactory(QnDataProviderFactory* dpFactory)
{
    m_dpFactory = dpFactory;
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
    Qn::MotionType motionType = Qn::MT_Default;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
        auto& regions = (*userAttributesLock)->motionRegions;
        while (regions.size() < channel)
            regions << QnMotionRegion();
        if (regions[channel] == mask)
            return;
        regions[channel] = mask;
        motionType = (*userAttributesLock)->motionType;
    }

    if (motionType != Qn::MT_SoftwareGrid)
        setMotionMaskPhysical(channel);

    emit motionRegionChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setMotionRegionList(const QList<QnMotionRegion>& maskList)
{
    NX_ASSERT(!getId().isNull());
    Qn::MotionType motionType = Qn::MT_Default;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );

        if( (*userAttributesLock)->motionRegions == maskList )
            return;
        (*userAttributesLock)->motionRegions = maskList;
        motionType = (*userAttributesLock)->motionType;
    }

    if (motionType != Qn::MT_SoftwareGrid)
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

bool QnSecurityCamResource::hasDualStreaming2() const {
    return m_cachedHasDualStreaming2.get();
}

nx::media::CameraMediaCapability QnSecurityCamResource::cameraMediaCapability() const
{
    return m_cachedCameraMediaCapabilities.get();
}

bool QnSecurityCamResource::hasDualStreaming() const
{
    const auto capabilities = cameraMediaCapability();
    if (!capabilities.isNull())
        return capabilities.hasDualStreaming;

    // Compatibility with version < 3.1.2
    QString val = getProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME);
    return val.toInt() > 0;
}

bool QnSecurityCamResource::isDtsBased() const {
    return m_cachedIsDtsBased.get();
}

bool QnSecurityCamResource::isAnalog() const {
    QString val = getProperty(Qn::ANALOG_PARAM_NAME);
    return val.toInt() > 0;
}

bool QnSecurityCamResource::isAnalogEncoder() const
{
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    return resourceData.value<bool>(lit("analogEncoder"));
}

CombinedSensorsDescription QnSecurityCamResource::combinedSensorsDescription() const
{
    const auto& value = getProperty(Qn::kCombinedSensorsDescriptionParamName);
    return QJson::deserialized<CombinedSensorsDescription>(value.toLatin1());
}

void QnSecurityCamResource::setCombinedSensorsDescription(
    const CombinedSensorsDescription& sensorsDescription)
{
    setProperty(Qn::kCombinedSensorsDescriptionParamName,
        QString::fromLatin1(QJson::serialized(sensorsDescription)));
}

bool QnSecurityCamResource::hasCombinedSensors() const
{
    return !combinedSensorsDescription().isEmpty();
}

bool QnSecurityCamResource::isEdge() const
{
    return QnMediaServerResource::isEdgeServer(getParentResource());
}

bool QnSecurityCamResource::isSharingLicenseInGroup() const
{
    if (getGroupId().isEmpty())
        return false; //< Not a multichannel device. Nothing to share
    if (!QnLicense::licenseTypeInfo(licenseType()).allowedToShareChannel)
        return false; //< Don't allow sharing for encoders e.t.c
    QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId());
    if (!resType)
        return false;
    return resType->hasParam(lit("canShareLicenseGroup"));
}

Qn::LicenseType QnSecurityCamResource::licenseType() const
{
    return m_cachedLicenseType.get();
}

Qn::StreamFpsSharingMethod QnSecurityCamResource::streamFpsSharingMethod() const {
    QString sval = getProperty(Qn::STREAM_FPS_SHARING_PARAM_NAME);
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
            setProperty(Qn::STREAM_FPS_SHARING_PARAM_NAME, lit("shareFps"));
            break;
        case Qn::NoFpsSharing:
            setProperty(Qn::STREAM_FPS_SHARING_PARAM_NAME, lit("noSharing"));
            break;
        default:
            setProperty(Qn::STREAM_FPS_SHARING_PARAM_NAME, lit("sharePixels"));
            break;
    }
}

QnIOPortDataList QnSecurityCamResource::getRelayOutputList() const {
    QnIOPortDataList result;
    QnIOPortDataList ports = getIOPorts();
    for (const auto& port: ports)
    {
        if (port.portType == Qn::PT_Output)
            result.push_back(port);
    }
    return result;
}

QnIOPortDataList QnSecurityCamResource::getInputPortList() const
{
    QnIOPortDataList result;
    QnIOPortDataList ports = getIOPorts();
    for (const auto& port: ports)
    {
        if (port.portType == Qn::PT_Input)
            result.push_back(port);
    }
    return result;
}

void QnSecurityCamResource::setIOPorts(const QnIOPortDataList& ports)
{
    setProperty(Qn::IO_SETTINGS_PARAM_NAME, QString::fromUtf8(QJson::serialized(ports)));
}

QnIOPortDataList QnSecurityCamResource::getIOPorts() const
{
    return QJson::deserialized<QnIOPortDataList>(getProperty(Qn::IO_SETTINGS_PARAM_NAME).toUtf8());
}

bool QnSecurityCamResource::setRelayOutputState(const QString& ouputID, bool activate, unsigned int autoResetTimeout)
{
    Q_UNUSED(ouputID)
    Q_UNUSED(activate)
    Q_UNUSED(autoResetTimeout)
    return false;
}

void QnSecurityCamResource::inputPortListenerAttached()
{
    QnMutexLocker lk( &m_initMutex );

    //if camera is not initialized yet, delayed input monitoring will start on initialization completion
    const int inputPortListenerCount = m_inputPortListenerCount.fetchAndAddOrdered( 1 );
    if( isInitialized() && (inputPortListenerCount == 0) )
        startInputPortMonitoringAsync( std::function<void(bool)>() );
    //if resource is not initialized, input port monitoring will start just after init() completion
}

void QnSecurityCamResource::inputPortListenerDetached()
{
    QnMutexLocker lk( &m_initMutex );

    if( m_inputPortListenerCount.load() <= 0 )
        return;

    int result = m_inputPortListenerCount.fetchAndAddOrdered( -1 );
    if( result == 1 )
        stopInputPortMonitoringAsync();
    else if( result <= 0 )
        m_inputPortListenerCount.fetchAndAddOrdered( 1 );   //no reduce below 0
}

void QnSecurityCamResource::at_initializedChanged()
{
    if( !isInitialized() )  //e.g., camera has been moved to a different server
        stopInputPortMonitoringAsync();  //stopping input monitoring

    emit licenseTypeChanged(toSharedPointer());
}

void QnSecurityCamResource::at_motionRegionChanged()
{
    if (flags() & Qn::foreigner)
        return;

    if (getMotionType() == Qn::MT_HardwareGrid || getMotionType() == Qn::MT_MotionWindow)
    {
    	QnConstResourceVideoLayoutPtr layout = getVideoLayout();
    	int numChannels = layout->channelCount();
        for (int i = 0; i < numChannels; ++i)
            setMotionMaskPhysical(i);
    }
}

int QnSecurityCamResource::motionWindowCount() const
{
    QString val = getProperty(Qn::MOTION_WINDOW_CNT_PARAM_NAME);
    return val.toInt();
}

int QnSecurityCamResource::motionMaskWindowCount() const
{
    QString val = getProperty(Qn::MOTION_MASK_WINDOW_CNT_PARAM_NAME);
    return val.toInt();
}

int QnSecurityCamResource::motionSensWindowCount() const
{
    QString val = getProperty(Qn::MOTION_SENS_WINDOW_CNT_PARAM_NAME);
    return val.toInt();
}


bool QnSecurityCamResource::hasTwoWayAudio() const
{
    return getCameraCapabilities().testFlag(Qn::AudioTransmitCapability);
}

bool QnSecurityCamResource::isAudioSupported() const
{
    const auto capabilities = cameraMediaCapability();
    if (!capabilities.isNull())
        return capabilities.hasAudio;

    // Compatibility with version < 3.1.2
    QString val = getProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME);
    return val.toInt() > 0;
}

Qn::MotionType QnSecurityCamResource::getDefaultMotionType() const
{
    Qn::MotionTypes value = supportedMotionType();
    if (value.testFlag(Qn::MT_SoftwareGrid))
        return Qn::MT_SoftwareGrid;

    if (value.testFlag(Qn::MT_HardwareGrid))
        return Qn::MT_HardwareGrid;

    if (value.testFlag(Qn::MT_MotionWindow))
        return Qn::MT_MotionWindow;

    return Qn::MT_NoMotion;
}

Qn::MotionTypes QnSecurityCamResource::supportedMotionType() const
{
    return m_cachedSupportedMotionType.get();
}

bool QnSecurityCamResource::hasMotion() const
{
    Qn::MotionType motionType = getDefaultMotionType();
    if (motionType == Qn::MT_SoftwareGrid)
    {
        return hasDualStreaming2()
            || (getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability)
            || !getProperty(QnMediaResource::motionStreamKey()).isEmpty();
    }
    return motionType != Qn::MT_NoMotion;
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
    if (value == Qn::MT_NoMotion)
        return value;

    if (value == Qn::MT_Default || !(supportedMotionType() & value))
        return getDefaultMotionType();

    return value;
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
    setProperty(Qn::CAMERA_CAPABILITIES_PARAM_NAME, static_cast<int>(capabilities));
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


QString QnSecurityCamResource::getGroupName() const
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

void QnSecurityCamResource::setGroupName(const QString& value)
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
    return getProperty( Qn::FIRMWARE_PARAM_NAME );
}

void QnSecurityCamResource::setFirmware(const QString &firmware)
{
    setProperty( Qn::FIRMWARE_PARAM_NAME, firmware );
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

nx::api::AnalyticsSupportedEvents QnSecurityCamResource::analyticsSupportedEvents() const
{
    return m_cachedAnalyticsSupportedEvents.get();
}

void QnSecurityCamResource::setAnalyticsSupportedEvents(
    const nx::api::AnalyticsSupportedEvents& eventsList)
{
    if (eventsList.isEmpty())
        setProperty(Qn::kAnalyticsDriversParamName, QVariant());
    else
        setProperty(Qn::kAnalyticsDriversParamName, QString::fromUtf8(QJson::serialized(eventsList)));
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

void QnSecurityCamResource::setScheduleDisabled(bool value)
{
    NX_ASSERT(!getId().isNull());
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
        if ((*userAttributesLock)->scheduleDisabled == value)
            return;
        (*userAttributesLock)->scheduleDisabled = value;
    }

    emit scheduleDisabledChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::isScheduleDisabled() const
{
    NX_ASSERT(!getId().isNull());
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    return (*userAttributesLock)->scheduleDisabled;
}

void QnSecurityCamResource::setLicenseUsed(bool value)
{
    /// TODO: #gdm Refactor licence management
    /*
    switch (licenseType())
    {
        case Qn::LC_IO:
        {
            QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
            if ((*userAttributesLock)->licenseUsed == value)
                return;
            (*userAttributesLock)->licenseUsed = value;
        }
        default:
            break;
    }
    */
    setScheduleDisabled(!value);
    emit licenseUsedChanged(::toSharedPointer(this));
}


bool QnSecurityCamResource::isLicenseUsed() const
{
    /// TODO: #gdm Refactor licence management
    /*
    switch (licenseType())
    {
        case Qn::LC_IO:
        {
            QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
            return (*userAttributesLock)->licenseUsed;
        }
        default:
            break;
    }
    */
    /* By default camera requires license when recording is enabled. */
    return !isScheduleDisabled();
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
    QnCameraUserAttributePool::ScopedLock userAttributesLock( userAttributesPool(), getId() );
    (*userAttributesLock)->audioEnabled = enabled;
}
bool QnSecurityCamResource::isAudioEnabled() const
{
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
    return hasCameraCapabilities(Qn::isDefaultPasswordCapability);
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

    if (result == Qn::CameraBackup_Disabled)
        return result;

    const auto& settings = commonModule()->globalSettings();

    auto value = settings->backupQualities();

    /* If backup is not configured on this camera, use 'Backup newly added cameras' value */
    if (result == Qn::CameraBackup_Default)
    {
        return settings->backupNewCamerasByDefault()
            ? value
            : Qn::CameraBackup_Disabled;
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

    m_cachedHasDualStreaming2.reset();
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
        case Qn::QualityLowest:
        case Qn::QualityLow:
            return kDefaultSecondStreamFpsMedium;
        case Qn::QualityNormal:
            return kDefaultSecondStreamFpsLow;
        case Qn::QualityHigh:
        case Qn::QualityHighest:
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
    const QList<QRegion>& regions,
    qint64 msStartTime,
    qint64 msEndTime,
    int detailLevel )
{
    Q_UNUSED( regions );
    Q_UNUSED( msStartTime );
    Q_UNUSED( msEndTime );
    Q_UNUSED( detailLevel );

    return QnTimePeriodList();
}

bool QnSecurityCamResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source)
{
    QnSecurityCamResource* camera = dynamic_cast<QnSecurityCamResource*>(source.data());
    if (!camera)
        return false;

    bool result = base_type::mergeResourcesIfNeeded(source);

    if (getGroupId() != camera->getGroupId())
    {
        setGroupId(camera->getGroupId());
        result = true; // groupID can be changed for onvif resource because if not auth info, maxChannels is not accessible
    }

    if (getGroupName().isEmpty() && getGroupName() != camera->getGroupName())
    {
        setGroupName(camera->getGroupName());
        result = true;
    }

    if (getModel() != camera->getModel() && !camera->getModel().isEmpty())
    {
        setModel(camera->getModel());
        result = true;
    }
    if (getVendor() != camera->getVendor() && !camera->getVendor().isEmpty())
    {
        setVendor(camera->getVendor());
        result = true;
    }
    if (getMAC() != camera->getMAC() && !camera->getMAC().isNull())
    {
        setMAC(camera->getMAC());
        result = true;
    }


    return result;
}

//QnMotionRegion QnSecurityCamResource::getMotionRegionNonSafe(int channel) const
//{
//    return m_motionMaskList[channel];
//}

Qn::MotionTypes QnSecurityCamResource::calculateSupportedMotionType() const
{
    QString val = getProperty(Qn::SUPPORTED_MOTION_PARAM_NAME);
    if (val.isEmpty())
        return Qn::MT_SoftwareGrid;

    Qn::MotionTypes result = Qn::MT_Default;
    for(const QString& str: val.split(L','))
    {
        QString s1 = str.toLower().trimmed();
        if (s1 == lit("hardwaregrid"))
            result |= Qn::MT_HardwareGrid;
        else if (s1 == lit("softwaregrid"))
            result |= Qn::MT_SoftwareGrid;
        else if (s1 == lit("motionwindow"))
            result |= Qn::MT_MotionWindow;
    }

    return result;
}

void QnSecurityCamResource::resetCachedValues()
{
    // TODO: #rvasilenko reset only required values on property changed (as in server resource).
    //resetting cached values
    m_cachedHasDualStreaming2.reset();
    m_cachedSupportedMotionType.reset();
    m_cachedCameraCapabilities.reset();
    m_cachedIsDtsBased.reset();
    m_motionType.reset();
    m_cachedIsIOModule.reset();
    m_cachedAnalyticsSupportedEvents.reset();
    m_cachedCameraMediaCapabilities.reset();
    m_cachedLicenseType.reset();
}

Qn::BitratePerGopType QnSecurityCamResource::bitratePerGopType() const
{
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    if (resourceData.value<bool>(Qn::FORCE_BITRATE_PER_GOP))
        return Qn::BPG_Predefined;

    if (getProperty(Qn::FORCE_BITRATE_PER_GOP).toInt() > 0)
        return Qn::BPG_User;

    return Qn::BPG_None;
}

bool QnSecurityCamResource::isIOModule() const
{
    return m_cachedIsIOModule.get();
}

#ifdef ENABLE_DATA_PROVIDERS
QnAudioTransmitterPtr QnSecurityCamResource::getAudioTransmitter()
{
    if (!isInitialized())
        return nullptr;
    return m_audioTransmitter;
}

#endif

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
    float lowEnd = 0.1f;
    float hiEnd = 1.0f;

    float qualityFactor = lowEnd + (hiEnd - lowEnd) * (quality - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);

    float resolutionFactor = 0.009f * pow(resolution.width() * resolution.height(), 0.7f);

    float frameRateFactor = fps / 1.0f;

    float result = qualityFactor*frameRateFactor * resolutionFactor;

    return qMax(192.0, result);
}

bool QnSecurityCamResource::captureEvent(const nx::vms::event::AbstractEventPtr& event)
{
    return false;
}

bool QnSecurityCamResource::doesEventComeFromAnalyticsDriver(nx::vms::event::EventType eventType) const
{
    return eventType == nx::vms::event::EventType::analyticsSdkEvent;
}

int QnSecurityCamResource::suggestBitrateKbps(const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const
{
    if (streamParams.bitrateKbps > 0)
    {
        auto result = streamParams.bitrateKbps;
        auto streamCapability = cameraMediaCapability().streamCapabilities.value(role);
        if (streamCapability.maxBitrateKbps > 0)
            result = qBound(streamCapability.minBitrateKbps, result, streamCapability.maxBitrateKbps);
        return result;
    }
    return suggestBitrateForQualityKbps(streamParams.quality, streamParams.resolution, streamParams.fps, role);
}

int QnSecurityCamResource::suggestBitrateForQualityKbps(Qn::StreamQuality quality, QSize resolution, int fps, Qn::ConnectionRole role) const
{
    auto bitrateCoefficient = [](Qn::StreamQuality quality)
    {
        switch (quality)
        {
        case Qn::StreamQuality::QualityLowest:
            return 0.66;
        case Qn::StreamQuality::QualityLow:
            return 0.8;
        case Qn::StreamQuality::QualityNormal:
            return 1.0;
        case Qn::StreamQuality::QualityHigh:
            return 2.0;
        case Qn::StreamQuality::QualityHighest:
            return 2.5;
        case Qn::StreamQuality::QualityPreSet:
        case Qn::StreamQuality::QualityNotDefined:
        default:
            return 1.0;
        }
    };
    if (role == Qn::CR_Default)
        role = Qn::CR_LiveVideo;
    auto streamCapability = cameraMediaCapability().streamCapabilities.value(role);
    if (streamCapability.defaultBitrateKbps > 0)
    {
        double coefficient = bitrateCoefficient(quality);
        const int bitrate = streamCapability.defaultBitrateKbps * coefficient;
        return qBound(
            (double)streamCapability.minBitrateKbps,
            bitrate * ((double)fps / streamCapability.defaultFps),
            (double)streamCapability.maxBitrateKbps);
    }


    auto result = rawSuggestBitrateKbps(quality, resolution, fps);

    if (bitratePerGopType() != Qn::BPG_None)
        result = result * (30.0 / (qreal)fps);

    return (int)result;
}

bool QnSecurityCamResource::setCameraCredentialsSync(
    const QAuthenticator& auth, QString* outErrorString)
{
    if (outErrorString)
        *outErrorString = lit("Operation is not permitted.");
    return false;
}
