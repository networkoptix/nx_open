#include "camera_resource.h"
#include "../dataprovider/live_stream_provider.h"
#include "resource_consumer.h"

QnVirtualCameraResource::QnVirtualCameraResource()
    : m_scheduleDisabled(true),
      m_audioEnabled(false)
{
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
    }
}

void QnVirtualCameraResource::setScheduleDisabled(bool disabled)
{
    m_scheduleDisabled = disabled;
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