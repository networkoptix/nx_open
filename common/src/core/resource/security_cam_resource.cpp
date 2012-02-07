#include "security_cam_resource.h"

#include "plugins/resources/archive/archive_stream_reader.h"
#include "../dataprovider/live_stream_provider.h"

static const char *property_descriptions[] = {
    QT_TRANSLATE_NOOP("QnResource", "Motion Mask"),
    QT_TRANSLATE_NOOP("QnResource", "Camera Scheduling")
};

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

    addFlag(live_cam);
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
        m_motionMask = other_casted->m_motionMask;
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

QRegion QnSecurityCamResource::getMotionMask() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_motionMask;
}

void QnSecurityCamResource::setMotionMask(const QRegion& mask)
{
    QMutexLocker mutexLocker(&m_mutex);
    if (m_motionMask == mask)
        return;
    m_motionMask = mask;
    emit motionMaskChanged(mask);
}

void QnSecurityCamResource::setScheduleTasks(const QnScheduleTaskList &scheduleTasks)
{
    m_scheduleTasks = scheduleTasks;
}

const QnScheduleTaskList &QnSecurityCamResource::getScheduleTasks() const
{
    return m_scheduleTasks;
}
