#include "security_cam_resource.h"

#include "plugins/resources/archive/archive_stream_reader.h"
#include "../dataprovider/live_stream_provider.h"


QnSecurityCamResource::QnSecurityCamResource()
    : QnMediaResource(),
      m_dpFactory(0),
      m_motionType(MT_Default)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QnMotionRegion>();
        qRegisterMetaType<QnScheduleTask>();
        qRegisterMetaType<QnScheduleTaskList>();
        metaTypesInitialized = true;
    }

    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_motionMaskList << QnMotionRegion();
    }

    addFlags(live_cam);
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

        const QnVideoResourceLayout* layout = getVideoLayout();
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
    if (!hasSuchParam("MaxFPS"))
    {
        //Q_ASSERT(false);
        return 15;
    }

    QVariant val;
    getParam("MaxFPS", val, QnDomainMemory);
    return val.toInt();
}

QSize QnSecurityCamResource::getMaxSensorSize()
{

    if (!hasSuchParam("MaxSensorWidth") || !hasSuchParam("MaxSensorHeight"))
    {
        Q_ASSERT(false);
        return QSize(0,0);
    }

    QVariant val_w, val_h;
    getParam("MaxSensorWidth", val_w, QnDomainMemory);
    getParam("MaxSensorHeight", val_h, QnDomainMemory);

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
        if (QnLiveStreamProvider* lsp = dynamic_cast<QnLiveStreamProvider*>(result))
        {
                lsp->setRole(role);
        }
        return result;

    }
    if (m_dpFactory)
        return m_dpFactory->createDataProviderInternal(toSharedPointer(), role);
    return 0;
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
        bool sameMask = true;
        for (int i = 0; i < CL_MAX_CHANNELS; ++i) 
        {
            sameMask &= m_motionMaskList[i] == maskList[i];
        }
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
    m_scheduleTasks = scheduleTasks;
}

const QnScheduleTaskList &QnSecurityCamResource::getScheduleTasks() const
{
    return m_scheduleTasks;
}

bool QnSecurityCamResource::hasDualStreaming() const
{
    if (!hasSuchParam("hasDualStreaming"))
    {
        //Q_ASSERT(false);
        return false;
    }

    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    this_casted->getParam("hasDualStreaming", val, QnDomainMemory);
    return val.toInt();
}

MotionType QnSecurityCamResource::getDefaultMotionType() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam("supportedMotion", val, QnDomainMemory))
    {
        QStringList vals = val.toString().split(',');
        QString s1 = vals[0].toLower();
        if (s1 == QString("hardwaregrid"))
            return MT_HardwareGrid;
        else if (s1 == QString("softwaregrid"))
            return MT_SoftwareGrid;
        else if (s1 == QString("motionwindow"))
            return MT_MotionWindow;
    }
    else {
        return MT_MotionWindow;
    }
}

int QnSecurityCamResource::motionWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam("motionWindowCnt", val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

int QnSecurityCamResource::motionMaskWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam("motionMaskWindowCnt", val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

int QnSecurityCamResource::motionSensWindowCount() const
{
    QVariant val;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    if (this_casted->getParam("motionSensWindowCnt", val, QnDomainMemory))
    {
        return val.toInt();
    }
    return 0;
}

bool QnSecurityCamResource::isAudioSupported() const
{
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);
    QVariant val;
    if (this_casted->getParam("isAudioSupported", val, QnDomainMemory))
        return val.toUInt() > 0;
    else
        return false;
}

MotionTypeFlags QnSecurityCamResource::supportedMotionType() const
{
    QVariant val;
    MotionTypeFlags result = MT_Default;
    QnSecurityCamResource* this_casted = const_cast<QnSecurityCamResource*>(this);

    if (this_casted->getParam("supportedMotion", val, QnDomainMemory))
    {
        QStringList vals = val.toString().split(',');
        foreach(const QString& str, vals)
        {
            QString s1 = str.toLower().trimmed();
            if (s1 == QString("hardwaregrid"))
                result |= MT_HardwareGrid;
            else if (s1 == QString("softwaregrid"))
                result |= MT_SoftwareGrid;
            else if (s1 == QString("motionwindow"))
                result |= MT_MotionWindow;
        }
        if (!hasDualStreaming())
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
