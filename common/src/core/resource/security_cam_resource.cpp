#include "security_cam_resource.h"

#include "common/common_meta_types.h"
#include "plugins/resources/archive/archive_stream_reader.h"

QnSecurityCamResource::QnSecurityCamResource()
    : QnMediaResource(),
      m_dpFactory(0),
      m_motionType(MT_Default)
{
    QnCommonMetaTypes::initilize();

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
    QnMediaResource::updateInner(other);

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
    if (!hasParam(QLatin1String("MaxFPS")))
    {
        //Q_ASSERT(false);
        return 15;
    }

    QVariant val;
    getParam(QLatin1String("MaxFPS"), val, QnDomainMemory);
    return val.toInt();
}

int QnSecurityCamResource::reservedSecondStreamFps()
{
    if (!hasParam(QLatin1String("reservedSecondStreamFps")))
    {
        //Q_ASSERT(false);
        return 2;
    }

    QVariant val;
    getParam(QLatin1String("reservedSecondStreamFps"), val, QnDomainMemory);
    return val.toInt();
}

QSize QnSecurityCamResource::getMaxSensorSize()
{

    if (!hasParam(QLatin1String("MaxSensorWidth")) || !hasParam(QLatin1String("MaxSensorHeight")))
    {
        Q_ASSERT(false);
        return QSize(0,0);
    }

    QVariant val_w, val_h;
    getParam(QLatin1String("MaxSensorWidth"), val_w, QnDomainMemory);
    getParam(QLatin1String("MaxSensorHeight"), val_h, QnDomainMemory);

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
    if (m_dpFactory)
        return m_dpFactory->createDataProviderInternal(toSharedPointer(), role);
    return 0;
}

bool QnSecurityCamResource::startInputPortMonitoring()
{
    return false;
}

void QnSecurityCamResource::stopInputPortMonitoring()
{
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
        if (m_motionType == MT_SoftwareGrid)
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
        if (m_motionType == MT_SoftwareGrid)
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
    if (!hasParam(QLatin1String("hasDualStreaming")))
    {
        //Q_ASSERT(false);
        return false;
    }

    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    this_casted->getParam(QLatin1String("hasDualStreaming"), val, QnDomainMemory);
    return val.toInt();
}

StreamFpsSharingMethod QnSecurityCamResource::streamFpsSharingMethod() const
{
    if (!hasParam(QLatin1String("streamFpsSharing")))
    {
        //Q_ASSERT(false);
        return sharePixels;
    }

    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    this_casted->getParam(QLatin1String("streamFpsSharing"), val, QnDomainMemory);

    QString sval = val.toString();

    if (sval == QLatin1String("shareFps"))
        return shareFps;

    if (sval == QLatin1String("noSharing"))
        return noSharing;

    return sharePixels;
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
    if( m_inputPortListenerCount.fetchAndAddOrdered( 1 ) == 0 )
        startInputPortMonitoring();
}

void QnSecurityCamResource::inputPortListenerDetached()
{
    if( m_inputPortListenerCount <= 0 )
        return;

    int result = m_inputPortListenerCount.fetchAndAddOrdered( -1 );
    if( result == 1 )
        stopInputPortMonitoring();
    else if( result <= 0 )
        m_inputPortListenerCount.fetchAndAddOrdered( 1 );   //no reduce below 0
}

MotionType QnSecurityCamResource::getCameraBasedMotionType() const
{
    MotionTypeFlags rez = supportedMotionType();
    if (rez & MT_HardwareGrid)
        return MT_HardwareGrid;
    else if (rez & MT_MotionWindow)
        return MT_MotionWindow;
    else
        return MT_NoMotion;
}

MotionType QnSecurityCamResource::getDefaultMotionType() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam(QLatin1String("supportedMotion"), val, QnDomainMemory))
    {
        QStringList vals = val.toString().split(QLatin1Char(','));
        for (int i = 0; i < vals.size(); ++i)
        {
            QString s1 = vals[i].toLower();
            if (s1 == QLatin1String("hardwaregrid"))
                return MT_HardwareGrid;
            else if (s1 == QLatin1String("softwaregrid") && hasDualStreaming())
                return MT_SoftwareGrid;
            else if (s1 == QLatin1String("motionwindow"))
                return MT_MotionWindow;
        }
        return MT_NoMotion;
    }
    else {
        return MT_MotionWindow;
    }
}

int QnSecurityCamResource::motionWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam(QLatin1String("motionWindowCnt"), val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

int QnSecurityCamResource::motionMaskWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam(QLatin1String("motionMaskWindowCnt"), val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

int QnSecurityCamResource::motionSensWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam(QLatin1String("motionSensWindowCnt"), val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

bool QnSecurityCamResource::isAudioSupported() const
{
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    QVariant val;
    if (this_casted->getParam(QLatin1String("isAudioSupported"), val, QnDomainMemory))
        return val.toUInt() > 0;
    else
        return false;
}

MotionTypeFlags QnSecurityCamResource::supportedMotionType() const
{
    QVariant val;
    MotionTypeFlags result = MT_Default;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);

    if (this_casted->getParam(QLatin1String("supportedMotion"), val, QnDomainMemory))
    {
        QStringList vals = val.toString().split(QLatin1Char(','));
        foreach(const QString& str, vals)
        {
            QString s1 = str.toLower().trimmed();
            if (s1 == QLatin1String("hardwaregrid"))
                result |= MT_HardwareGrid;
            else if (s1 == QLatin1String("softwaregrid"))
                result |= MT_SoftwareGrid;
            else if (s1 == QLatin1String("motionwindow"))
                result |= MT_MotionWindow;
        }
        if (!hasDualStreaming() && !(getCameraCapabilities() &  PrimaryStreamSoftMotionCapability))
            result &= ~MT_SoftwareGrid;
    }
    else {
        result = MT_NoMotion;
    }
    return result;
}

MotionType QnSecurityCamResource::getMotionType()
{
    if (m_motionType == MT_Default)
        m_motionType = getDefaultMotionType();
    return m_motionType;
}

void QnSecurityCamResource::setMotionType(MotionType value)
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

QnSecurityCamResource::CameraCapabilities QnSecurityCamResource::getCameraCapabilities() const
{
    QVariant mediaVariant;
    const_cast<QnSecurityCamResource *>(this)->getParam(QLatin1String("cameraCapabilities"), mediaVariant, QnDomainMemory); // TODO: const_cast? get rid of it!
    return static_cast<CameraCapabilities>(mediaVariant.toInt());
}

void QnSecurityCamResource::setCameraCapabilities(CameraCapabilities capabilities) {
    setParam(QLatin1String("cameraCapabilities"), static_cast<int>(capabilities), QnDomainDatabase);

    // TODO: #1.5 this signal won't be emitted if parameter was changed directly (e.g. as a result of resource update).

    // TODO: we don't check whether they have actually changed. This better be fixed.
    emit cameraCapabilitiesChanged(::toSharedPointer(this));
}

void QnSecurityCamResource::setCameraCapability(CameraCapability capability, bool value) {
    setCameraCapabilities(value ? (getCameraCapabilities() | capability) : (getCameraCapabilities() & ~capability));
}
