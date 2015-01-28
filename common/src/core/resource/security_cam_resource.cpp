#include "security_cam_resource.h"

#include <QtCore/QMutexLocker>

#include <api/global_settings.h>

#include <utils/serialization/lexical.h>

#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>

#include <core/dataprovider/data_provider_factory.h>

#include "user_resource.h"
#include "common/common_module.h"

#include <recording/time_period_list.h>
#include "camera_user_attribute_pool.h"
#include "core/resource/media_server_resource.h"
#include "resource_data.h"

#define SAFE(expr) {QMutexLocker lock(&m_mutex); expr;}


namespace {
    static const int defaultMaxFps = 15;
    static const int defaultReservedSecondStreamFps = 2;
    static const Qn::StreamFpsSharingMethod defaultStreamFpsSharingMethod = Qn::PixelsFpsSharing;
    static const Qn::MotionType defaultMotionType = Qn::MT_MotionWindow;

    static const int defaultSecondStreamFpsLow = 2;
    static const int defaultSecondStreamFpsMedium = 7;
    static const int defaultSecondStreamFpsHigh = 12;
}

//const int PRIMARY_ENCODER_INDEX = 0;
//const int SECONDARY_ENCODER_INDEX = 1;

QnSecurityCamResource::QnSecurityCamResource(): 
    m_dpFactory(0),
    m_recActionCnt(0),
    m_statusFlags(Qn::CSF_NoFlags),
    m_advancedWorking(false),
    m_manuallyAdded(false)
{
    addFlags(Qn::live_cam);

    connect(this, &QnResource::initializedChanged, this, &QnSecurityCamResource::at_initializedChanged, Qt::DirectConnection);
    connect(this, &QnSecurityCamResource::motionRegionChanged, this, &QnSecurityCamResource::at_motionRegionChanged, Qt::DirectConnection);

    QnMediaResource::initMediaResource();
}

QString QnSecurityCamResource::getName() const
{
    if( getId().isNull() )
        return QnResource::getName();

    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    if( !(*userAttributesLock)->name.isEmpty() )
        return (*userAttributesLock)->name;
    return QnResource::getName();
}

void QnSecurityCamResource::setName( const QString& name )
{
    QString oldName = getName();
    if (oldName == name)
        return;

    setCameraName(name);
    emit nameChanged(toSharedPointer(this));
}

void QnSecurityCamResource::setCameraName( const QString& newCameraName )
{
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
        (*userAttributesLock)->name = newCameraName;
    }
    QnResource::setName( newCameraName );
}

bool QnSecurityCamResource::isGroupPlayOnly() const {
    return hasParam(lit("groupplay"));
}

const QnResource* QnSecurityCamResource::toResource() const {
    return this;
}

QnResource* QnSecurityCamResource::toResource() {
    return this;
}

const QnResourcePtr QnSecurityCamResource::toResourcePtr() const {
    return toSharedPointer();
}

QnResourcePtr QnSecurityCamResource::toResourcePtr() {
    return toSharedPointer();
}

QnSecurityCamResource::~QnSecurityCamResource() {
}

void QnSecurityCamResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) 
{
    QnNetworkResource::updateInner(other, modifiedFields);
    QnMediaResource::updateInner(other, modifiedFields);

    QnSecurityCamResourcePtr other_casted = qSharedPointerDynamicCast<QnSecurityCamResource>(other);
    if (other_casted)
    {
        QnConstResourceVideoLayoutPtr layout = getVideoLayout();

        if (other_casted->m_groupId != m_groupId)
            modifiedFields << "groupIdChanged";

        if (other_casted->m_groupName != m_groupName)
            modifiedFields << "groupNameChanged";

        m_groupId = other_casted->m_groupId;
        m_groupName = other_casted->m_groupName;
        m_statusFlags = other_casted->m_statusFlags;
        m_manuallyAdded = other_casted->m_manuallyAdded;
        m_model = other_casted->m_model;
        m_vendor = other_casted->m_vendor;
    }
}

int QnSecurityCamResource::getMaxFps() const {
    QString value = getProperty(lit("MaxFPS"));
    return value.isNull() ? defaultMaxFps : value.toInt();
}

int QnSecurityCamResource::reservedSecondStreamFps() const {
    QString value = getProperty(lit("reservedSecondStreamFps"));
    return value.isNull() ? defaultReservedSecondStreamFps : value.toInt();
}

#ifdef ENABLE_DATA_PROVIDERS
QnAbstractStreamDataProvider* QnSecurityCamResource::createDataProviderInternal(Qn::ConnectionRole role) {
    if (role == Qn::CR_LiveVideo || role == Qn::CR_Default || role == Qn::CR_SecondaryLiveVideo)
    {

        if (role == Qn::CR_SecondaryLiveVideo && !hasDualStreaming2())
            return 0;

        QnAbstractStreamDataProvider* result = createLiveDataProvider();
        if (result)
            result->setRole(role);
        return result;

    }
    else if (role == Qn::CR_Archive) {
        QnAbstractStreamDataProvider* result = createArchiveDataProvider();
        if (result)
            return result;
    }

    if (m_dpFactory)
        return m_dpFactory->createDataProviderInternal(toSharedPointer(), role);
    return 0;
}
#endif // ENABLE_DATA_PROVIDERS

void QnSecurityCamResource::initializationDone() 
{
    //m_initMutex is locked down the stack
    QnNetworkResource::initializationDone();
    if( m_inputPortListenerCount.load() > 0 )
        startInputPortMonitoringAsync( std::function<void(bool)>() );
}

bool QnSecurityCamResource::startInputPortMonitoringAsync( std::function<void(bool)>&& /*completionHandler*/ ) {
    return false;
}

void QnSecurityCamResource::stopInputPortMonitoringAsync() {
}

bool QnSecurityCamResource::isInputPortMonitored() const {
    return false;
}

void QnSecurityCamResource::setDataProviderFactory(QnDataProviderFactory* dpFactory) {
    m_dpFactory = dpFactory;
}

QList<QnMotionRegion> QnSecurityCamResource::getMotionRegionList() const {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->motionRegions;
}

QRegion QnSecurityCamResource::getMotionMask(int channel) const {
    return getMotionRegion(channel).getMotionMask();
}

QnMotionRegion QnSecurityCamResource::getMotionRegion(int channel) const {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->motionRegions[channel];
}

void QnSecurityCamResource::setMotionRegion(const QnMotionRegion& mask, int channel) {
    Qn::MotionType motionType = Qn::MT_Default;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );

        if ((*userAttributesLock)->motionRegions[channel] == mask)
            return;
        (*userAttributesLock)->motionRegions[channel] = mask;
        motionType = (*userAttributesLock)->motionType;
    }

    if (motionType != Qn::MT_SoftwareGrid)
        setMotionMaskPhysical(channel);
}

void QnSecurityCamResource::setMotionRegionList(const QList<QnMotionRegion>& maskList) {
    Qn::MotionType motionType = Qn::MT_Default;
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );

        if( (*userAttributesLock)->motionRegions == maskList )
            return;
        (*userAttributesLock)->motionRegions = maskList;
        motionType = (*userAttributesLock)->motionType;
    }

    if (motionType != Qn::MT_SoftwareGrid) {
        for (int i = 0; i < getVideoLayout()->channelCount(); ++i)
            setMotionMaskPhysical(i);
    }
}

void QnSecurityCamResource::setScheduleTasks(const QnScheduleTaskList& scheduleTasks) {
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
        (*userAttributesLock)->scheduleTasks = scheduleTasks;
    }
    emit scheduleTasksChanged(::toSharedPointer(this));
}

QnScheduleTaskList QnSecurityCamResource::getScheduleTasks() const {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->scheduleTasks;
}

bool QnSecurityCamResource::hasDualStreaming2() const {
    return hasDualStreaming() && secondaryStreamQuality() != Qn::SSQualityDontUse;
}

bool QnSecurityCamResource::hasDualStreaming() const {
    QString val = getProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME);
    return val.toInt() > 0;
}

bool QnSecurityCamResource::isDtsBased() const {
    QString val = getProperty(Qn::DTS_PARAM_NAME);
    return val.toInt() > 0;
}

bool QnSecurityCamResource::isAnalog() const {
    QString val = getProperty(Qn::ANALOG_PARAM_NAME);
    return val.toInt() > 0;
}

bool QnSecurityCamResource::isAnalogEncoder() const {
    const QnSecurityCamResourcePtr ptr = toSharedPointer(const_cast<QnSecurityCamResource*> (this));
    QnResourceData resourceData = qnCommon->dataPool()->data(ptr);
    return resourceData.value<bool>(lit("analogEncoder"));
}

bool QnSecurityCamResource::isEdge() const {
    return QnMediaServerResource::isEdgeServer(qnResPool->getResourceById(getParentId()));
}

Qn::LicenseType QnSecurityCamResource::licenseType() const 
{
    QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId());
    if (resType && resType->getManufacture() == lit("VMAX"))
        return Qn::LC_VMAX;
    else if (isAnalogEncoder())
        return Qn::LC_AnalogEncoder; // AnalogEncoder should have priority over Analog type because of analog type is deprecated (DW-CP04 has both analog and analogEncoder params)
    else if (isAnalog())
        return Qn::LC_Analog;
    else if (isEdge())
        return Qn::LC_Edge;
    else
        return Qn::LC_Professional;
}


Qn::StreamFpsSharingMethod QnSecurityCamResource::streamFpsSharingMethod() const {
    QString sval = getProperty(Qn::STREAM_FPS_SHARING_PARAM_NAME);
    if (sval.isEmpty())
        return defaultStreamFpsSharingMethod;

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

QStringList QnSecurityCamResource::getRelayOutputList() const {
    return QStringList();
}

QStringList QnSecurityCamResource::getInputPortList() const {
    return QStringList();
}

bool QnSecurityCamResource::setRelayOutputState(const QString& ouputID, bool activate, unsigned int autoResetTimeout) {
    Q_UNUSED(ouputID)
    Q_UNUSED(activate)
    Q_UNUSED(autoResetTimeout)
    return false;
}

void QnSecurityCamResource::inputPortListenerAttached() {
    QMutexLocker lk( &m_initMutex );

    //if camera is not initialized yet, delayed input monitoring will start on initialization completion
    const int inputPortListenerCount = m_inputPortListenerCount.fetchAndAddOrdered( 1 );
    if( isInitialized() && (inputPortListenerCount == 0) )
        startInputPortMonitoringAsync( std::function<void(bool)>() );
    //if resource is not initialized, input port monitoring will start just after initInternal completion
}

void QnSecurityCamResource::inputPortListenerDetached() {
    QMutexLocker lk( &m_initMutex );
 
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

bool QnSecurityCamResource::isAudioSupported() const {
    QString val = getProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME);
    if (val.toInt() > 0)
        return true;

    val = getProperty(Qn::FORCED_IS_AUDIO_SUPPORTED_PARAM_NAME);
    return val.toInt() > 0;
}

Qn::MotionType QnSecurityCamResource::getCameraBasedMotionType() const {
    Qn::MotionTypes rez = supportedMotionType();
    if (rez & Qn::MT_HardwareGrid)
        return Qn::MT_HardwareGrid;
    else if (rez & Qn::MT_MotionWindow)
        return Qn::MT_MotionWindow;
    else
        return Qn::MT_NoMotion;
}

Qn::MotionType QnSecurityCamResource::getDefaultMotionType() const 
{
    Qn::MotionTypes value = supportedMotionType();
    if (value & Qn::MT_HardwareGrid)
        return Qn::MT_HardwareGrid;
    else if (value & Qn::MT_SoftwareGrid)
        return Qn::MT_SoftwareGrid;
    else if (value & Qn::MT_MotionWindow)
        return Qn::MT_MotionWindow;
    else
        return Qn::MT_NoMotion;
}

Qn::MotionTypes QnSecurityCamResource::supportedMotionType() const {
    QString val = getProperty(Qn::SUPPORTED_MOTION_PARAM_NAME);
    if (val.isEmpty())
        return Qn::MT_NoMotion;

    Qn::MotionTypes result = Qn::MT_Default;
    for(const QString& str: val.split(L',')) {
        QString s1 = str.toLower().trimmed();
        if (s1 == lit("hardwaregrid"))
            result |= Qn::MT_HardwareGrid;
        else if (s1 == lit("softwaregrid"))
            result |= Qn::MT_SoftwareGrid;
        else if (s1 == lit("motionwindow"))
            result |= Qn::MT_MotionWindow;
    }
    //if ((!hasDualStreaming() || secondaryStreamQuality() == Qn::SSQualityDontUse) && !(getCameraCapabilities() &  Qn::PrimaryStreamSoftMotionCapability))
    //    result &= ~Qn::MT_SoftwareGrid;

    return result;
}

bool QnSecurityCamResource::hasMotion() const {
    Qn::MotionType motionType = getDefaultMotionType();
    if (motionType == Qn::MT_SoftwareGrid)
        return hasDualStreaming2() || (getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability);
    else
        return motionType != Qn::MT_NoMotion;
}

Qn::MotionType QnSecurityCamResource::getMotionType() const {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    Qn::MotionType value = (*userAttributesLock)->motionType;
    if (value == Qn::MT_NoMotion)
        return value;
    if (value == Qn::MT_Default || !(supportedMotionType() & value))
        return getDefaultMotionType();
    else
        return value;
}

void QnSecurityCamResource::setMotionType(Qn::MotionType value) {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    (*userAttributesLock)->motionType = value;
}

Qn::CameraCapabilities QnSecurityCamResource::getCameraCapabilities() const {
    QString val = getProperty(Qn::CAMERA_CAPABILITIES_PARAM_NAME);
    return static_cast<Qn::CameraCapabilities>(val.toInt());
}

bool QnSecurityCamResource::hasCameraCapabilities(Qn::CameraCapabilities capabilities) const {
    return getCameraCapabilities() & capabilities;
}

void QnSecurityCamResource::setCameraCapabilities(Qn::CameraCapabilities capabilities) {
    setProperty(Qn::CAMERA_CAPABILITIES_PARAM_NAME, static_cast<int>(capabilities));
}

void QnSecurityCamResource::setCameraCapability(Qn::CameraCapability capability, bool value) {
    setCameraCapabilities(value ? (getCameraCapabilities() | capability) : (getCameraCapabilities() & ~capability));
}

bool QnSecurityCamResource::isRecordingEventAttached() const {
    return m_recActionCnt > 0;
}

void QnSecurityCamResource::recordingEventAttached() {
    m_recActionCnt++;
}

void QnSecurityCamResource::recordingEventDetached() {
    m_recActionCnt = qMax(0, m_recActionCnt-1);
}

QString QnSecurityCamResource::getGroupName() const {
    SAFE(return m_groupName)
}

void QnSecurityCamResource::setGroupName(const QString& value) {
    {
        QMutexLocker locker(&m_mutex);
        if(m_groupName == value)
            return;
        m_groupName = value;
    }

    emit groupNameChanged(::toSharedPointer(this));
}

QString QnSecurityCamResource::getGroupId() const {
    SAFE(return m_groupId)
}

void QnSecurityCamResource::setGroupId(const QString& value) {
    {
        QMutexLocker locker(&m_mutex);
        if(m_groupId == value)
            return;
        m_groupId = value;
    }

    emit groupIdChanged(::toSharedPointer(this));   
}

QString QnSecurityCamResource::getModel() const {
    SAFE(return m_model)
}

void QnSecurityCamResource::setModel(const QString &model) {
    SAFE(m_model = model)
}

QString QnSecurityCamResource::getFirmware() const {
    return getProperty(lit("firmware"));
}

void QnSecurityCamResource::setFirmware(const QString &firmware) {
    setProperty(lit("firmware"), firmware);
}

QString QnSecurityCamResource::getVendor() const {
    SAFE(return m_vendor);

    // This code is commented for a reason. We want to know if vendor is empty. --Elric
    //SAFE(if (!m_vendor.isEmpty()) return m_vendor)    //calculated on the server
    //
    //QnResourceTypePtr resourceType = qnResTypePool->getResourceType(getTypeId());
    //return resourceType ? resourceType->getManufacture() : QString(); //estimated value
}

void QnSecurityCamResource::setVendor(const QString& value) {
    SAFE(m_vendor = value)
}

void QnSecurityCamResource::setMaxDays(int value)
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    (*userAttributesLock)->maxDays = value;
}

int QnSecurityCamResource::maxDays() const
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->maxDays;
}

void QnSecurityCamResource::setPreferedServerId(const QnUuid& value)
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    (*userAttributesLock)->preferedServerId = value;
}

QnUuid QnSecurityCamResource::preferedServerId() const
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->preferedServerId;
}

void QnSecurityCamResource::setMinDays(int value)
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    (*userAttributesLock)->minDays = value;
}

int QnSecurityCamResource::minDays() const
{
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->minDays;
}

void QnSecurityCamResource::setScheduleDisabled(bool value) {
    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
        if ((*userAttributesLock)->scheduleDisabled == value)
            return;
        (*userAttributesLock)->scheduleDisabled = value;
    }

    emit scheduleDisabledChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::isScheduleDisabled() const {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->scheduleDisabled;
}

void QnSecurityCamResource::setAudioEnabled(bool enabled) {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    (*userAttributesLock)->audioEnabled = enabled;
}

bool QnSecurityCamResource::isAudioEnabled() const {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->audioEnabled;
}

bool QnSecurityCamResource::isAdvancedWorking() const {
    return m_advancedWorking;
}

void QnSecurityCamResource::setAdvancedWorking(bool value) {
    m_advancedWorking = value;
}

bool QnSecurityCamResource::isManuallyAdded() const {
    return m_manuallyAdded;
}

void QnSecurityCamResource::setManuallyAdded(bool value) {
    m_manuallyAdded = value;
}

void QnSecurityCamResource::setSecondaryStreamQuality(Qn::SecondStreamQuality quality) {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    (*userAttributesLock)->secondaryQuality = quality;
}

Qn::SecondStreamQuality QnSecurityCamResource::secondaryStreamQuality() const {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->secondaryQuality;
}

void QnSecurityCamResource::setCameraControlDisabled(bool value) {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    (*userAttributesLock)->cameraControlDisabled = value;
}

bool QnSecurityCamResource::isCameraControlDisabled() const {
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
    return (*userAttributesLock)->cameraControlDisabled;
}

int QnSecurityCamResource::desiredSecondStreamFps() const {
    switch (secondaryStreamQuality()) {
    case Qn::SSQualityMedium:
        return defaultSecondStreamFpsMedium;
    case Qn::SSQualityLow: 
        return defaultSecondStreamFpsLow;
    case Qn::SSQualityHigh:
        return defaultSecondStreamFpsHigh;
    default:
        break;
    }
    return defaultSecondStreamFpsMedium;
}

Qn::StreamQuality QnSecurityCamResource::getSecondaryStreamQuality() const {
    if (secondaryStreamQuality() != Qn::SSQualityHigh)
        return Qn::QualityLowest;
    else
        return Qn::QualityNormal;
}

Qn::CameraStatusFlags QnSecurityCamResource::statusFlags() const {
    return m_statusFlags;
}

bool QnSecurityCamResource::hasStatusFlags(Qn::CameraStatusFlag value) const
{
    return m_statusFlags & value;
}

void QnSecurityCamResource::setStatusFlags(Qn::CameraStatusFlags value) {
    m_statusFlags = value;
}

void QnSecurityCamResource::addStatusFlags(Qn::CameraStatusFlag value) {
    m_statusFlags |= value;
}

void QnSecurityCamResource::removeStatusFlags(Qn::CameraStatusFlag value) {
    m_statusFlags &= ~value;
}


bool QnSecurityCamResource::needCheckIpConflicts() const {
    return getChannel() == 0 && !hasCameraCapabilities(Qn::ShareIpCapability);
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

bool QnSecurityCamResource::mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) {
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

    if (getModel() != camera->getModel() && !camera->getModel().isEmpty()) {
        setModel(camera->getModel());
        result = true;
    }
    if (getVendor() != camera->getVendor() && !camera->getVendor().isEmpty()) {
        setVendor(camera->getVendor());
        result = true;
    }
    if (getMAC() != camera->getMAC() && !camera->getMAC().isNull()) {
        setMAC(camera->getMAC());
        result = true;
    }


    return result;
}

//QnMotionRegion QnSecurityCamResource::getMotionRegionNonSafe(int channel) const
//{
//    return m_motionMaskList[channel];
//}
