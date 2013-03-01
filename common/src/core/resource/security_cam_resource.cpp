
#include "security_cam_resource.h"

#include <QMutexLocker>

#include "plugins/resources/archive/archive_stream_reader.h"


QnSecurityCamResource::QnSecurityCamResource(): 
    m_dpFactory(0),
    m_motionType(Qn::MT_Default),
    m_recActionCnt(0)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionMaskList << QnMotionRegion();

    addFlags(live_cam);

    connect(this, SIGNAL(disabledChanged(const QnResourcePtr &)), this, SLOT(at_disabledChanged()), Qt::DirectConnection);
}

QnSecurityCamResource::~QnSecurityCamResource()
{
}

void QnSecurityCamResource::updateInner(QnResourcePtr other)
{
    base_type::updateInner(other);

    QnSecurityCamResourcePtr other_casted = qSharedPointerDynamicCast<QnSecurityCamResource>(other);
    if (other_casted)
    {

        const QnResourceVideoLayout* layout = getVideoLayout();
        int numChannels = layout->numberOfChannels();

        m_motionType = other_casted->m_motionType;

        for (int i = 0; i < numChannels; ++i) 
            setMotionRegion(other_casted->m_motionMaskList[i], QnDomainPhysical, i);
        m_scheduleTasks = other_casted->m_scheduleTasks;
    }
}

QString QnSecurityCamResource::oemName() const
{
    return manufacture();
}

int QnSecurityCamResource::getMaxFps()
{
    if (!hasParam(lit("MaxFPS")))
    {
        //Q_ASSERT(false);
        return 15;
    }

    QVariant val;
    getParam(lit("MaxFPS"), val, QnDomainMemory);
    return val.toInt();
}

int QnSecurityCamResource::reservedSecondStreamFps()
{
    if (!hasParam(lit("reservedSecondStreamFps")))
    {
        //Q_ASSERT(false);
        return 2;
    }

    QVariant val;
    getParam(lit("reservedSecondStreamFps"), val, QnDomainMemory);
    return val.toInt();
}

QSize QnSecurityCamResource::getMaxSensorSize()
{

    if (!hasParam(lit("MaxSensorWidth")) || !hasParam(lit("MaxSensorHeight")))
    {
        Q_ASSERT(false);
        return QSize(0,0);
    }

    QVariant val_w, val_h;
    getParam(lit("MaxSensorWidth"), val_w, QnDomainMemory);
    getParam(lit("MaxSensorHeight"), val_h, QnDomainMemory);

    return QSize(val_w.toInt(), val_h.toInt());

}

QRect QnSecurityCamResource::getCroping(QnDomain /*domain*/)
{
    return QRect(0, 0, 100, 100);
}

void QnSecurityCamResource::setCroping(QRect croping, QnDomain /*domain*/)
{
    setCropingPhysical(croping);
}

QnAbstractStreamDataProvider* QnSecurityCamResource::createDataProviderInternal(QnResource::ConnectionRole role)
{
    if (role == QnResource::Role_LiveVideo || role == QnResource::Role_Default || role == QnResource::Role_SecondaryLiveVideo)
    {

        if (role == QnResource::Role_SecondaryLiveVideo && !hasDualStreaming())
            return 0;

        QnAbstractStreamDataProvider* result = createLiveDataProvider();
        result->setRole(role);
        /*
        if (QnLiveStreamProvider* lsp = dynamic_cast<QnLiveStreamProvider*>(result))
        {
                lsp->setRole(role);
        }
        */
        return result;

    }
    else if (role == QnResource::Role_Archive) {
        
        QnAbstractArchiveDelegate* archiveDelegate = createArchiveDelegate();
        if (archiveDelegate) {
            QnArchiveStreamReader* archiveReader = new QnArchiveStreamReader(toSharedPointer());
            archiveReader->setArchiveDelegate(archiveDelegate);
            return archiveReader;
        }
    }

    if (m_dpFactory)
        return m_dpFactory->createDataProviderInternal(toSharedPointer(), role);
    return 0;
}

void QnSecurityCamResource::initializationDone()
{
    QMutexLocker lk( &m_mutex );

    if( m_inputPortListenerCount > 0 )
        startInputPortMonitoring();
}

bool QnSecurityCamResource::startInputPortMonitoring()
{
    return false;
}

void QnSecurityCamResource::stopInputPortMonitoring()
{
}

bool QnSecurityCamResource::isInputPortMonitored() const
{
    return false;
}

void QnSecurityCamResource::setDataProviderFactory(QnDataProviderFactory* dpFactory)
{
    m_dpFactory = dpFactory;
}

QList<QnMotionRegion> QnSecurityCamResource::getMotionRegionList() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_motionMaskList;
}

QRegion QnSecurityCamResource::getMotionMask(int channel) const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_motionMaskList[channel].getMotionMask();
}

QnMotionRegion QnSecurityCamResource::getMotionRegion(int channel) const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_motionMaskList[channel];
}

void QnSecurityCamResource::setMotionRegion(const QnMotionRegion& mask, QnDomain domain, int channel)
{
    {
        QMutexLocker mutexLocker(&m_mutex);
        if (m_motionMaskList[channel] == mask)
            return;
        m_motionMaskList[channel] = mask;
    }

    if (domain == QnDomainPhysical) 
    {
        if (m_motionType == Qn::MT_SoftwareGrid)
        {
            ;
        }
        else {
            setMotionMaskPhysical(channel);
        }
    }
}

void QnSecurityCamResource::setMotionRegionList(const QList<QnMotionRegion>& maskList, QnDomain domain)
{
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

    if (domain == QnDomainPhysical)
    {
        if (m_motionType == Qn::MT_SoftwareGrid)
        {
            ;
        }
        else 
        {
            for (int i = 0; i < getVideoLayout()->numberOfChannels(); ++i)
                setMotionMaskPhysical(i);
        }
    }
}

void QnSecurityCamResource::setScheduleTasks(const QnScheduleTaskList &scheduleTasks)
{
    {
        QMutexLocker lock(&m_mutex);
        m_scheduleTasks = scheduleTasks;
    }

    emit scheduleTasksChanged(::toSharedPointer(this));
}

const QnScheduleTaskList QnSecurityCamResource::getScheduleTasks() const
{
    QMutexLocker lock(&m_mutex);
    return m_scheduleTasks;
}

bool QnSecurityCamResource::hasDualStreaming() const
{
    if (!hasParam(lit("hasDualStreaming")))
    {
        //Q_ASSERT(false);
        return false;
    }

    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    this_casted->getParam(lit("hasDualStreaming"), val, QnDomainMemory);
    return val.toInt();
}

bool QnSecurityCamResource::isDtsBased() const
{
    if (!hasParam(lit("dts")))
        return false;

    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    this_casted->getParam(lit("dts"), val, QnDomainMemory);
    return val.toInt();
}

Qn::StreamFpsSharingMethod QnSecurityCamResource::streamFpsSharingMethod() const
{
    if (!hasParam(lit("streamFpsSharing")))
    {
        //Q_ASSERT(false);
        return Qn::sharePixels;
    }

    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    this_casted->getParam(lit("streamFpsSharing"), val, QnDomainMemory);

    QString sval = val.toString();

    if (sval == lit("shareFps"))
        return Qn::shareFps;

    if (sval == lit("noSharing"))
        return Qn::noSharing;

    return Qn::sharePixels;
}

QStringList QnSecurityCamResource::getRelayOutputList() const
{
    return QStringList();
}

QStringList QnSecurityCamResource::getInputPortList() const
{
    return QStringList();
}

bool QnSecurityCamResource::setRelayOutputState(
    const QString& /*ouputID*/,
    bool /*activate*/,
    unsigned int /*autoResetTimeout*/ )
{
    return false;
}

void QnSecurityCamResource::inputPortListenerAttached()
{
    QMutexLocker lk( &m_mutex );

    //if camera is not initialized yet, delayed input monitoring will start on initialization completion
    if( m_inputPortListenerCount.fetchAndAddOrdered( 1 ) == 0 )
        startInputPortMonitoring();
}

void QnSecurityCamResource::inputPortListenerDetached()
{
    QMutexLocker lk( &m_mutex );
 
    if( m_inputPortListenerCount <= 0 )
        return;

    int result = m_inputPortListenerCount.fetchAndAddOrdered( -1 );
    if( result == 1 )
        stopInputPortMonitoring();
    else if( result <= 0 )
        m_inputPortListenerCount.fetchAndAddOrdered( 1 );   //no reduce below 0
}

Qn::MotionType QnSecurityCamResource::getCameraBasedMotionType() const
{
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
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam(lit("supportedMotion"), val, QnDomainMemory))
    {
        QStringList vals = val.toString().split(QLatin1Char(','));
        for (int i = 0; i < vals.size(); ++i)
        {
            QString s1 = vals[i].toLower();
            if (s1 == lit("hardwaregrid"))
                return Qn::MT_HardwareGrid;
            else if (s1 == lit("softwaregrid") && hasDualStreaming())
                return Qn::MT_SoftwareGrid;
            else if (s1 == lit("motionwindow"))
                return Qn::MT_MotionWindow;
        }
        return Qn::MT_NoMotion;
    }
    else {
        return Qn::MT_MotionWindow;
    }
}

int QnSecurityCamResource::motionWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam(lit("motionWindowCnt"), val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

int QnSecurityCamResource::motionMaskWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam(lit("motionMaskWindowCnt"), val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

int QnSecurityCamResource::motionSensWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam(lit("motionSensWindowCnt"), val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

bool QnSecurityCamResource::isAudioSupported() const
{
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    QVariant val;
    if (this_casted->getParam(lit("isAudioSupported"), val, QnDomainMemory))
        return val.toUInt() > 0;
    else
        return false;
}

Qn::MotionTypes QnSecurityCamResource::supportedMotionType() const
{
    QVariant val;
    Qn::MotionTypes result = Qn::MT_Default;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);

    if (this_casted->getParam(lit("supportedMotion"), val, QnDomainMemory))
    {
        QStringList vals = val.toString().split(QLatin1Char(','));
        foreach(const QString& str, vals)
        {
            QString s1 = str.toLower().trimmed();
            if (s1 == lit("hardwaregrid"))
                result |= Qn::MT_HardwareGrid;
            else if (s1 == lit("softwaregrid"))
                result |= Qn::MT_SoftwareGrid;
            else if (s1 == lit("motionwindow"))
                result |= Qn::MT_MotionWindow;
        }
        if (!hasDualStreaming() && !(getCameraCapabilities() &  Qn::PrimaryStreamSoftMotionCapability))
            result &= ~Qn::MT_SoftwareGrid;
    }
    else {
        result = Qn::MT_NoMotion;
    }
    return result;
}

Qn::MotionType QnSecurityCamResource::getMotionType()
{
    if (m_motionType == Qn::MT_Default)
        m_motionType = getDefaultMotionType();
    return m_motionType;
}

void QnSecurityCamResource::setMotionType(Qn::MotionType value)
{
    m_motionType = value;
}

void QnSecurityCamResource::at_disabledChanged()
{
    if(hasFlags(QnResource::foreigner))
        return; // we do not own camera

    if(isDisabled())
        stopInputPortMonitoring();
    else
        startInputPortMonitoring();
}

Qn::CameraCapabilities QnSecurityCamResource::getCameraCapabilities() const
{
    QVariant mediaVariant;
    const_cast<QnSecurityCamResource *>(this)->getParam(QLatin1String("cameraCapabilities"), mediaVariant, QnDomainMemory); // TODO: const_cast? get rid of it!
    return Qn::undeprecate(static_cast<Qn::CameraCapabilities>(mediaVariant.toInt()));
}

bool QnSecurityCamResource::hasCameraCapabilities(Qn::CameraCapabilities capabilities) const
{
    return getCameraCapabilities() & capabilities;
}

void QnSecurityCamResource::setCameraCapabilities(Qn::CameraCapabilities capabilities) {
    setParam(lit("cameraCapabilities"), static_cast<int>(capabilities), QnDomainDatabase);
}

void QnSecurityCamResource::setCameraCapability(Qn::CameraCapability capability, bool value) {
    setCameraCapabilities(value ? (getCameraCapabilities() | capability) : (getCameraCapabilities() & ~capability));
}

bool QnSecurityCamResource::setParam(const QString &name, const QVariant &val, QnDomain domain) {
    bool result = base_type::setParam(name, val, domain);
    if(result && name == lit("cameraCapabilities"))
        emit cameraCapabilitiesChanged(::toSharedPointer(this)); // TODO: we don't check whether they have actually changed. This better be fixed.
    return result;
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
