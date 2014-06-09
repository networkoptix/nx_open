#include "security_cam_resource.h"

#include <QtCore/QMutexLocker>

#include <api/global_settings.h>

#include <utils/serialization/lexical.h>

#include <core/resource_management/resource_pool.h>

#include <business/business_event_connector.h>

#include "user_resource.h"
#include "common/common_module.h"

#include <recording/time_period_list.h>

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

QnSecurityCamResource::QnSecurityCamResource(): 
    m_dpFactory(0),
    m_motionType(Qn::MT_Default),
    m_recActionCnt(0),
    m_secondaryQuality(Qn::SSQualityMedium),
    m_cameraControlDisabled(false),
    m_statusFlags(0),
    m_scheduleDisabled(true),
    m_audioEnabled(false),
    m_advancedWorking(false),
    m_manuallyAdded(false),
    m_minDays(0),
    m_maxDays(0)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionMaskList << QnMotionRegion();

    addFlags(live_cam);

    m_cameraControlDisabled = !QnGlobalSettings::instance()->isCameraSettingsOptimizationEnabled();

    connect(this, &QnResource::parentIdChanged, this, &QnSecurityCamResource::at_parentIdChanged, Qt::DirectConnection);

    QnMediaResource::initMediaResource();

    // TODO: #AK this is a wrong place for this connect call.
    // You should listen to changes in resource pool instead.
    if(QnBusinessEventConnector::instance()) {
        connect(
            this, SIGNAL(cameraInput(QnResourcePtr, const QString&, bool, qint64)), 
            QnBusinessEventConnector::instance(), SLOT(at_cameraInput(QnResourcePtr, const QString&, bool, qint64)) );
    }
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
        int numChannels = layout->channelCount();

        m_motionType = other_casted->m_motionType;
        
        bool motionRegionChanged = false;
        for (int i = 0; i < numChannels; ++i) {
            if (m_motionMaskList[i] == other_casted->m_motionMaskList[i])
                continue;
            setMotionRegion(other_casted->m_motionMaskList[i], QnDomainPhysical, i);
            motionRegionChanged = true;
        }
        if (motionRegionChanged)
            modifiedFields << "motionRegionChanged";

        m_scheduleTasks = other_casted->m_scheduleTasks;
        m_groupId = other_casted->m_groupId;
        m_groupName = other_casted->m_groupName;
        m_secondaryQuality = other_casted->m_secondaryQuality;
        m_cameraControlDisabled = other_casted->m_cameraControlDisabled;
        m_statusFlags = other_casted->m_statusFlags;
        m_scheduleDisabled = other_casted->m_scheduleDisabled;
        m_audioEnabled = other_casted->m_audioEnabled;
        m_manuallyAdded = other_casted->m_manuallyAdded;
        m_model = other_casted->m_model;
        m_vendor = other_casted->m_vendor;
        m_minDays = other_casted->m_minDays;
        m_maxDays = other_casted->m_maxDays;
    }
}

int QnSecurityCamResource::getMaxFps() const {
    QVariant val;
    if (!getParam(lit("MaxFPS"), val, QnDomainMemory))
        return defaultMaxFps;
    return val.toInt();
}

int QnSecurityCamResource::reservedSecondStreamFps() const {
    QVariant val;
    if (!getParam(lit("reservedSecondStreamFps"), val, QnDomainMemory))
        return defaultReservedSecondStreamFps;
    return val.toInt();
}

QSize QnSecurityCamResource::getMaxSensorSize() const {
    QVariant val_w, val_h;
    if (!getParam(lit("MaxSensorWidth"), val_w, QnDomainMemory) ||
            !getParam(lit("MaxSensorHeight"), val_h, QnDomainMemory))
        return QSize(0, 0);
    return QSize(val_w.toInt(), val_h.toInt());
}

QnAbstractStreamDataProvider* QnSecurityCamResource::createDataProviderInternal(QnResource::ConnectionRole role) {
    if (role == QnResource::Role_LiveVideo || role == QnResource::Role_Default || role == QnResource::Role_SecondaryLiveVideo)
    {

        if (role == QnResource::Role_SecondaryLiveVideo && !hasDualStreaming2())
            return 0;

        QnAbstractStreamDataProvider* result = createLiveDataProvider();
        result->setRole(role);
        return result;

    }
    else if (role == QnResource::Role_Archive) {
        QnAbstractStreamDataProvider* result = createArchiveDataProvider();
        if (result)
            return result;
    }

    if (m_dpFactory)
        return m_dpFactory->createDataProviderInternal(toSharedPointer(), role);
    return 0;
}

void QnSecurityCamResource::initializationDone() {
    if( m_inputPortListenerCount.load() > 0 )
        startInputPortMonitoring();
}

bool QnSecurityCamResource::startInputPortMonitoring() {
    return false;
}

void QnSecurityCamResource::stopInputPortMonitoring() {
}

bool QnSecurityCamResource::isInputPortMonitored() const {
    return false;
}

void QnSecurityCamResource::setDataProviderFactory(QnDataProviderFactory* dpFactory) {
    m_dpFactory = dpFactory;
}

QList<QnMotionRegion> QnSecurityCamResource::getMotionRegionList() const {
    SAFE(return m_motionMaskList)
}

QRegion QnSecurityCamResource::getMotionMask(int channel) const {
    SAFE(return m_motionMaskList[channel].getMotionMask())
}

QnMotionRegion QnSecurityCamResource::getMotionRegion(int channel) const {
    SAFE(return m_motionMaskList[channel])
}

void QnSecurityCamResource::setMotionRegion(const QnMotionRegion& mask, QnDomain domain, int channel) {
    {
        QMutexLocker mutexLocker(&m_mutex);
        if (m_motionMaskList[channel] == mask)
            return;
        m_motionMaskList[channel] = mask;
    }

    if (domain == QnDomainPhysical && m_motionType != Qn::MT_SoftwareGrid)
        setMotionMaskPhysical(channel);
}

void QnSecurityCamResource::setMotionRegionList(const QList<QnMotionRegion>& maskList, QnDomain domain) {
    {
        QMutexLocker mutexLocker(&m_mutex);
        bool sameMask = maskList.size() == m_motionMaskList.size();
        if(sameMask)
            for (int i = 0; i < maskList.size(); ++i) 
                sameMask &= m_motionMaskList[i] == maskList[i];
        if (sameMask)
            return;
        m_motionMaskList = maskList;
    }

    if (domain == QnDomainPhysical && m_motionType != Qn::MT_SoftwareGrid) {
        for (int i = 0; i < getVideoLayout()->channelCount(); ++i)
            setMotionMaskPhysical(i);
    }
}

void QnSecurityCamResource::setScheduleTasks(const QnScheduleTaskList &scheduleTasks) {
    SAFE(m_scheduleTasks = scheduleTasks)
    emit scheduleTasksChanged(::toSharedPointer(this));
}

const QnScheduleTaskList QnSecurityCamResource::getScheduleTasks() const {
    SAFE(return m_scheduleTasks)
}

bool QnSecurityCamResource::hasDualStreaming2() const {
    return hasDualStreaming() && secondaryStreamQuality() != Qn::SSQualityDontUse;
}

bool QnSecurityCamResource::hasDualStreaming() const {
    QVariant val;
    if (!getParam(lit("hasDualStreaming"), val, QnDomainMemory))
        return false;
    return val.toBool();
}

bool QnSecurityCamResource::isDtsBased() const {
    QVariant val;
    if (!getParam(lit("dts"), val, QnDomainMemory))
        return false;
    return val.toBool();
}

bool QnSecurityCamResource::isAnalog() const {
    QVariant val;
    if (!getParam(lit("analog"), val, QnDomainMemory))
        return false;
    return val.toBool();
}

Qn::StreamFpsSharingMethod QnSecurityCamResource::streamFpsSharingMethod() const {
    QVariant val;
    if (!getParam(lit("streamFpsSharing"), val, QnDomainMemory))
        return defaultStreamFpsSharingMethod;

    QString sval = val.toString();
    if (sval == lit("shareFps"))
        return Qn::BasicFpsSharing;
    if (sval == lit("noSharing"))
        return Qn::NoFpsSharing;
    return Qn::PixelsFpsSharing;
}

void QnSecurityCamResource::setStreamFpsSharingMethod(Qn::StreamFpsSharingMethod value) 
{
    QString strVal;
    if (value == Qn::BasicFpsSharing)
        strVal = lit("shareFps");
    else if (value == Qn::NoFpsSharing)
        strVal = lit("noSharing");
    else
        strVal = lit("sharePixels");
    setParam(lit("streamFpsSharing"), strVal, QnDomainDatabase);
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
    if( m_inputPortListenerCount.fetchAndAddOrdered( 1 ) == 0 )
        startInputPortMonitoring();
}

void QnSecurityCamResource::inputPortListenerDetached() {
    QMutexLocker lk( &m_initMutex );
 
    if( m_inputPortListenerCount.load() <= 0 )
        return;

    int result = m_inputPortListenerCount.fetchAndAddOrdered( -1 );
    if( result == 1 )
        stopInputPortMonitoring();
    else if( result <= 0 )
        m_inputPortListenerCount.fetchAndAddOrdered( 1 );   //no reduce below 0
}

void QnSecurityCamResource::at_parentIdChanged() 
{
    if(getParentId() != qnCommon->moduleGUID())
        stopInputPortMonitoring();
    else
        startInputPortMonitoring();
}

int QnSecurityCamResource::motionWindowCount() const {
    QVariant val;
    if (!getParam(lit("motionWindowCnt"), val, QnDomainMemory))
        return 0;
    return val.toInt();
}

int QnSecurityCamResource::motionMaskWindowCount() const {
    QVariant val;
    if (!getParam(lit("motionMaskWindowCnt"), val, QnDomainMemory))
        return 0;
    return val.toInt();
}

int QnSecurityCamResource::motionSensWindowCount() const {
    QVariant val;
    if (!getParam(lit("motionSensWindowCnt"), val, QnDomainMemory))
        return 0;
    return val.toInt();
}

bool QnSecurityCamResource::isAudioSupported() const {
    QVariant val;
    if (!getParam(lit("isAudioSupported"), val, QnDomainMemory))
        return false;
    if (val.toBool())
        return true;

    if (!getParam(lit("forcedIsAudioSupported"), val, QnDomainMemory))
        return false;

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

Qn::MotionType QnSecurityCamResource::getDefaultMotionType() const {
    QVariant val;
    if (!getParam(lit("supportedMotion"), val, QnDomainMemory))
        return defaultMotionType;

    foreach(const QString &s, val.toString().split(QLatin1Char(','))) {
        QString s1 = s.toLower();
        if (s1 == lit("hardwaregrid"))
            return Qn::MT_HardwareGrid;
        else if (s1 == lit("softwaregrid") && hasDualStreaming2())
            return Qn::MT_SoftwareGrid;
        else if (s1 == lit("motionwindow"))
            return Qn::MT_MotionWindow;
    }
    return Qn::MT_NoMotion;
}

Qn::MotionTypes QnSecurityCamResource::supportedMotionType() const {
    QVariant val;
    if (!getParam(lit("supportedMotion"), val, QnDomainMemory))
        return Qn::MT_NoMotion;

    Qn::MotionTypes result = Qn::MT_Default;
    foreach(const QString& str, val.toString().split(QLatin1Char(','))) {
        QString s1 = str.toLower().trimmed();
        if (s1 == lit("hardwaregrid"))
            result |= Qn::MT_HardwareGrid;
        else if (s1 == lit("softwaregrid"))
            result |= Qn::MT_SoftwareGrid;
        else if (s1 == lit("motionwindow"))
            result |= Qn::MT_MotionWindow;
    }
    if ((!hasDualStreaming() || secondaryStreamQuality() == Qn::SSQualityDontUse) && !(getCameraCapabilities() &  Qn::PrimaryStreamSoftMotionCapability))
        result &= ~Qn::MT_SoftwareGrid;

    return result;
}

Qn::MotionType QnSecurityCamResource::getMotionType() {
    if (m_motionType == Qn::MT_Default)
        m_motionType = getDefaultMotionType();
    return m_motionType;
}

void QnSecurityCamResource::setMotionType(Qn::MotionType value) {
    m_motionType = value;
}

Qn::CameraCapabilities QnSecurityCamResource::getCameraCapabilities() const {
    QVariant val;
    if (!getParam(QLatin1String("cameraCapabilities"), val, QnDomainMemory))
        return Qn::NoCapabilities;
    return static_cast<Qn::CameraCapabilities>(val.toInt());
}

bool QnSecurityCamResource::hasCameraCapabilities(Qn::CameraCapabilities capabilities) const {
    return getCameraCapabilities() & capabilities;
}

void QnSecurityCamResource::setCameraCapabilities(Qn::CameraCapabilities capabilities) {
    setParam(lit("cameraCapabilities"), static_cast<int>(capabilities), QnDomainDatabase);
}

void QnSecurityCamResource::setCameraCapability(Qn::CameraCapability capability, bool value) {
    setCameraCapabilities(value ? (getCameraCapabilities() | capability) : (getCameraCapabilities() & ~capability));
}

void QnSecurityCamResource::parameterValueChangedNotify(const QnParam &param) {
    if (param.name() == lit("cameraCapabilities"))
        emit cameraCapabilitiesChanged(::toSharedPointer(this));

    base_type::parameterValueChangedNotify(param);
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
    SAFE(m_groupId = value)
}

QString QnSecurityCamResource::getModel() const {
    SAFE(return m_model)
}

void QnSecurityCamResource::setModel(const QString &model) {
    SAFE(m_model = model)
}

QString QnSecurityCamResource::getFirmware() const {
    QVariant val;
    if (!getParam(lit("firmware"), val, QnDomainMemory))
        return QString();
    return val.toString();
}

void QnSecurityCamResource::setFirmware(const QString &firmware) {
    setParam(lit("firmware"), firmware, QnDomainDatabase);
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
    SAFE(m_maxDays = value)
}

int QnSecurityCamResource::maxDays() const
{
    SAFE(return m_maxDays);
}

void QnSecurityCamResource::setMinDays(int value)
{
    SAFE(m_minDays = value)
}

int QnSecurityCamResource::minDays() const
{
    SAFE(return m_minDays);
}

void QnSecurityCamResource::setScheduleDisabled(bool value) {
    {
        QMutexLocker locker(&m_mutex);
        if(m_scheduleDisabled == value)
            return;
        m_scheduleDisabled = value;
    }

    emit scheduleDisabledChanged(::toSharedPointer(this));
}

bool QnSecurityCamResource::isScheduleDisabled() const {
    return m_scheduleDisabled;
}

void QnSecurityCamResource::setAudioEnabled(bool enabled) {
    m_audioEnabled = enabled;
}

bool QnSecurityCamResource::isAudioEnabled() const {
    return m_audioEnabled;
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
    m_secondaryQuality = quality;
}

Qn::SecondStreamQuality QnSecurityCamResource::secondaryStreamQuality() const {
    return m_secondaryQuality;
}

void QnSecurityCamResource::setCameraControlDisabled(bool value) {
    m_cameraControlDisabled = value;
}

bool QnSecurityCamResource::isCameraControlDisabled() const {
    return m_cameraControlDisabled;
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

QnSecurityCamResource::StatusFlags QnSecurityCamResource::statusFlags() const {
    return m_statusFlags;
}

bool QnSecurityCamResource::hasStatusFlags(StatusFlags value) const {
    return m_statusFlags & value;
}

void QnSecurityCamResource::setStatusFlags(StatusFlags value) {
    m_statusFlags = value;
}

void QnSecurityCamResource::addStatusFlags(StatusFlags value) {
    m_statusFlags |= value;
}

void QnSecurityCamResource::removeStatusFlags(StatusFlags value) {
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
    QnSecurityCamResourcePtr camera = source.dynamicCast<QnSecurityCamResource>();
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

    return result;
}
