#include "security_cam_resource.h"

#include "plugins/resources/archive/archive_stream_reader.h"
#include "../dataprovider/live_stream_provider.h"


QnSecurityCamResource::QnSecurityCamResource()
    : QnMediaResource(),
      m_dpFactory(0)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QRegion>();
        qRegisterMetaType<QnScheduleTask>();
        qRegisterMetaType<QnScheduleTaskList>();
        metaTypesInitialized = true;
    }

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionMaskList << QRegion();

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
        for (int i = 0; i < CL_MAX_CHANNELS; ++i)
            setMotionMask(other_casted->m_motionMaskList[i], QnDomainPhysical, i);
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

QList<QRegion> QnSecurityCamResource::getMotionMaskList() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_motionMaskList;
}

QRegion QnSecurityCamResource::getMotionMask(int channel) const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_motionMaskList[channel];
}

void QnSecurityCamResource::setMotionMask(const QRegion& mask, QnDomain domain, int channel)
{
    {
        QMutexLocker mutexLocker(&m_mutex);
        if (m_motionMaskList[channel] == mask)
            return;
        m_motionMaskList[channel] = mask;
    }

    if (domain == QnDomainPhysical)
        setMotionMaskPhysical(channel);
}

void QnSecurityCamResource::setMotionMaskList(const QList<QRegion>& maskList, QnDomain domain)
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
        for (int i = 0; i < getVideoLayout()->numberOfChannels(); ++i)
            setMotionMaskPhysical(i);
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
    return true;
}