#include "camera_resource.h"
#include "core/dataprovider/live_stream_provider.h"
#include "resource_consumer.h"
#include "common/common_meta_types.h"

QnVirtualCameraResource::QnVirtualCameraResource()
    : m_scheduleDisabled(true),
      m_audioEnabled(false),
      m_manuallyAdded(false),
      m_advancedWorking(false),
      m_dtsFactory(0)
{
    QnCommonMetaTypes::initilize();
}

QnPhysicalCameraResource::QnPhysicalCameraResource(): QnVirtualCameraResource()
{
    setFlags(local_live_cam);
}

int QnPhysicalCameraResource::getPrimaryStreamDesiredFps() const
{
#ifdef _DEBUG
    debugCheck();
#endif

    QMutexLocker mutex(&m_consumersMtx);
    foreach(QnResourceConsumer* consumer, m_consumers)
    {
        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
        if (!lp)
            continue;

        if (lp->getRole() != QnResource::Role_SecondaryLiveVideo)
        {
            // must be a primary source 
            return lp->getFps();
        }
    }

    return 0;
}

int QnPhysicalCameraResource::getPrimaryStreamRealFps() const
{
    // will be implemented one day
    return getPrimaryStreamDesiredFps();
}

int QnPhysicalCameraResource::suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const
{
    // I assume for a QnQualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a QnQualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*10;
    int lowEnd = 1024*1;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;

    int result = lowEnd + (hiEnd - lowEnd) * (q - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(128,result);
}

void QnPhysicalCameraResource::onPrimaryFpsUpdated(int newFps)
{
#ifdef _DEBUG
    debugCheck();
#endif

    QMutexLocker mutex(&m_consumersMtx);
    foreach(QnResourceConsumer* consumer, m_consumers)
    {
        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
        if (!lp)
            continue;

        if (lp->getRole() == QnResource::Role_SecondaryLiveVideo)
        {
            lp->onPrimaryFpsUpdated(newFps);
            return;
        }
    }

        
}

#ifdef _DEBUG
void QnPhysicalCameraResource::debugCheck() const
{
    int countTotal = 0;
    int countPrimary = 0;

    QMutexLocker mutex(&m_consumersMtx);
    foreach(QnResourceConsumer* consumer, m_consumers)
    {
        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
        if (!lp)
            continue;

        ++countTotal;

        if (lp->getRole() != QnResource::Role_SecondaryLiveVideo)
        {
            // must be a primary source 
            ++countPrimary;
        }
    }

    Q_ASSERT(countTotal<=2); // more than 2 stream readers connected ?
    Q_ASSERT(countPrimary<=1); // more than 1 primary source ? 

}
#endif

void QnVirtualCameraResource::updateInner(QnResourcePtr other)
{
    QnNetworkResource::updateInner(other);
    QnSecurityCamResource::updateInner(other);

    QnVirtualCameraResourcePtr camera = other.dynamicCast<QnVirtualCameraResource>();
    if (camera)
    {
        m_scheduleDisabled = camera->isScheduleDisabled();
        m_audioEnabled = camera->isAudioEnabled();
        m_manuallyAdded = camera->isManuallyAdded();
    }
}

void QnVirtualCameraResource::setScheduleDisabled(bool disabled)
{
    QMutexLocker locker(&m_mutex);

    if(m_scheduleDisabled == disabled)
        return;

    m_scheduleDisabled = disabled;

    locker.unlock();
    emit scheduleDisabledChanged(::toSharedPointer(this));
}

bool QnVirtualCameraResource::isScheduleDisabled() const
{
    return m_scheduleDisabled;
}

void QnVirtualCameraResource::setAudioEnabled(bool enabled)
{
    m_audioEnabled = enabled;
}

bool QnVirtualCameraResource::isAudioEnabled() const
{
    return m_audioEnabled;
}

bool QnVirtualCameraResource::isManuallyAdded() const
{
    return m_manuallyAdded;
}
void QnVirtualCameraResource::setManuallyAdded(bool value)
{
    m_manuallyAdded = value;
}


bool QnVirtualCameraResource::isAdvancedWorking() const
{
    return m_advancedWorking;
}

void QnVirtualCameraResource::setAdvancedWorking(bool value)
{
    m_advancedWorking = value;
}

QnAbstractDTSFactory* QnVirtualCameraResource::getDTSFactory()
{
    return m_dtsFactory;
}

void QnVirtualCameraResource::setDTSFactory(QnAbstractDTSFactory* factory)
{
    m_dtsFactory = factory;
}

void QnVirtualCameraResource::lockDTSFactory()
{
    m_mutex.lock();
}

void QnVirtualCameraResource::unLockDTSFactory()
{
    m_mutex.unlock();
}

QnVirtualCameraResource::CameraCapabilities QnVirtualCameraResource::getCameraCapabilities()
{
    QVariant mediaVariant;
    getParam(QLatin1String("cameraCapabilities"), mediaVariant, QnDomainMemory);
    return (CameraCapabilities) mediaVariant.toInt();
}

void QnVirtualCameraResource::addCameraCapabilities(CameraCapabilities value)
{
    value |= getCameraCapabilities();
    int valueInt = (int) value;
    setParam(QLatin1String("cameraCapabilities"), valueInt, QnDomainDatabase);
}
