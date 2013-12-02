
#include "security_cam_resource.h"
#include <QtCore/QMutexLocker>

#include <business/business_event_connector.h>
#include "api/app_server_connection.h"


#define SAFE(expr) {QMutexLocker lock(&m_mutex); expr;}

namespace {
    static const int defaultMaxFps = 15;
    static const int defaultReservedSecondStreamFps = 2;
    static const Qn::StreamFpsSharingMethod defaultStreamFpsSharingMethod = Qn::sharePixels;
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
    m_manuallyAdded(false)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionMaskList << QnMotionRegion();

    addFlags(live_cam);

    m_cameraControlDisabled = !QnAppServerConnectionFactory::allowCameraChanges();

    connect(this, SIGNAL(disabledChanged(const QnResourcePtr &)), this, SLOT(at_disabledChanged()), Qt::DirectConnection);

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

void QnSecurityCamResource::updateInner(QnResourcePtr other) {
    QnNetworkResource::updateInner(other);
    QnMediaResource::updateInner(other);

    QnSecurityCamResourcePtr other_casted = qSharedPointerDynamicCast<QnSecurityCamResource>(other);
    if (other_casted)
    {
        const QnResourceVideoLayout* layout = getVideoLayout();
        int numChannels = layout->channelCount();

        m_motionType = other_casted->m_motionType;
        for (int i = 0; i < numChannels; ++i) 
            setMotionRegion(other_casted->m_motionMaskList[i], QnDomainPhysical, i);
        m_scheduleTasks = other_casted->m_scheduleTasks;
        m_groupId = other_casted->m_groupId;
        m_groupName = other_casted->m_groupName;
        m_secondaryQuality = other_casted->m_secondaryQuality;
        m_cameraControlDisabled = other_casted->m_cameraControlDisabled;
        m_statusFlags = other_casted->m_statusFlags;
        m_scheduleDisabled = other_casted->isScheduleDisabled();
        m_audioEnabled = other_casted->isAudioEnabled();
        m_manuallyAdded = other_casted->isManuallyAdded();
        m_model = other_casted->m_model;
        m_firmware = other_casted->m_firmware;
        m_vendor = other_casted->m_vendor;
    }
}

QString QnSecurityCamResource::getVendorInternal() const {
    return getDriverName();
}

int QnSecurityCamResource::getMaxFps() {
    QVariant val;
    if (!getParam(lit("MaxFPS"), val, QnDomainMemory))
        return defaultMaxFps;
    return val.toInt();
}

int QnSecurityCamResource::reservedSecondStreamFps() {
    QVariant val;
    if (!getParam(lit("reservedSecondStreamFps"), val, QnDomainMemory))
        return defaultReservedSecondStreamFps;
    return val.toInt();
}

QSize QnSecurityCamResource::getMaxSensorSize() {
    QVariant val_w, val_h;
    if (!getParam(lit("MaxSensorWidth"), val_w, QnDomainMemory) ||
            !getParam(lit("MaxSensorHeight"), val_h, QnDomainMemory))
        return QSize(0, 0);
    return QSize(val_w.toInt(), val_h.toInt());
}

QRect QnSecurityCamResource::getCropping(QnDomain domain) const {
    Q_UNUSED(domain)
    return QRect(0, 0, 100, 100);
}

void QnSecurityCamResource::setCropping(QRect cropping, QnDomain domain) {
    Q_UNUSED(domain)
    setCroppingPhysical(cropping);
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

bool QnSecurityCamResource::hasDualStreaming2() {
    return hasDualStreaming() && secondaryStreamQuality() != Qn::SSQualityDontUse;
}

bool QnSecurityCamResource::hasDualStreaming() {
    QVariant val;
    if (!getParam(lit("hasDualStreaming"), val, QnDomainMemory))
        return false;
    return val.toInt();
}

bool QnSecurityCamResource::isDtsBased() {
    QVariant val;
    if (!getParam(lit("dts"), val, QnDomainMemory))
        return false;
    return val.toInt();
}

bool QnSecurityCamResource::isAnalog() {
    QVariant val;
    if (!getParam(lit("analog"), val, QnDomainMemory))
        return false;
    return val.toInt();
}

Qn::StreamFpsSharingMethod QnSecurityCamResource::streamFpsSharingMethod() {
    QVariant val;
    if (!getParam(lit("streamFpsSharing"), val, QnDomainMemory))
        return defaultStreamFpsSharingMethod;

    QString sval = val.toString();
    if (sval == lit("shareFps"))
        return Qn::shareFps;
    if (sval == lit("noSharing"))
        return Qn::noSharing;
    return Qn::sharePixels;
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

void QnSecurityCamResource::at_disabledChanged() {
    if(hasFlags(QnResource::foreigner))
        return; // we do not own camera

    if(isDisabled())
        stopInputPortMonitoring();
    else
        startInputPortMonitoring();
}

int QnSecurityCamResource::motionWindowCount() {
    QVariant val;
    if (!getParam(lit("motionWindowCnt"), val, QnDomainMemory))
        return 0;
    return val.toInt();
}

int QnSecurityCamResource::motionMaskWindowCount() {
    QVariant val;
    if (!getParam(lit("motionMaskWindowCnt"), val, QnDomainMemory))
        return 0;
    return val.toInt();
}

int QnSecurityCamResource::motionSensWindowCount() {
    QVariant val;
    if (!getParam(lit("motionSensWindowCnt"), val, QnDomainMemory))
        return 0;
    return val.toInt();
}

bool QnSecurityCamResource::isAudioSupported() {
    QVariant val;
    if (!getParam(lit("isAudioSupported"), val, QnDomainMemory))
        return false;
    return val.toUInt() > 0;
}

Qn::MotionType QnSecurityCamResource::getCameraBasedMotionType() {
    Qn::MotionTypes rez = supportedMotionType();
    if (rez & Qn::MT_HardwareGrid)
        return Qn::MT_HardwareGrid;
    else if (rez & Qn::MT_MotionWindow)
        return Qn::MT_MotionWindow;
    else
        return Qn::MT_NoMotion;
}

Qn::MotionType QnSecurityCamResource::getDefaultMotionType() {
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

Qn::MotionTypes QnSecurityCamResource::supportedMotionType() {
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

Qn::CameraCapabilities QnSecurityCamResource::getCameraCapabilities() {
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

bool QnSecurityCamResource::setParam(const QString &name, const QVariant &val, QnDomain domain) {
    if (name != lit("cameraCapabilities"))
        return QnResource::setParam(name, val, domain);

    QVariant old;
    Qn::CameraCapabilities oldCapabilities = getParam(QLatin1String("cameraCapabilities"), old, domain)
            ? static_cast<Qn::CameraCapabilities>(old.toInt())
            : Qn::NoCapabilities;
    Qn::CameraCapabilities newCapabilities = static_cast<Qn::CameraCapabilities>(val.toInt());
    if (oldCapabilities == newCapabilities)
        return true;

    bool result = QnResource::setParam(name, val, domain);
    if (result)
        emit cameraCapabilitiesChanged(::toSharedPointer(this));
    return result;
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
    SAFE(m_groupName = value)
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
    SAFE(return m_firmware)
}

void QnSecurityCamResource::setFirmware(const QString &firmware) {
    SAFE(m_firmware = firmware)
}

QString QnSecurityCamResource::getVendor() const {
    SAFE(if (!m_vendor.isEmpty()) return m_vendor)    //calculated on the server

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(getTypeId());
    return resourceType ? resourceType->getManufacture() : QString(); //estimated value
}

void QnSecurityCamResource::setVendor(const QString& value) {
    SAFE(m_vendor = value)
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
    return getChannel() == 0 && !hasCameraCapabilities(Qn::shareIpCapability);
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
