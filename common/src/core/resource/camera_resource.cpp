#include "camera_resource.h"
#include "../dataprovider/live_stream_provider.h"
#include "resource_consumer.h"

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
    int countLp = 0;
    int countPLp = 0;

    QMutexLocker mutex(&m_consumersMtx);
    foreach(QnResourceConsumer* consumer, m_consumers)
    {
        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
        if (!lp)
            continue;

        ++countLp;

        if (lp->getRole() != QnResource::Role_SecondaryLiveVideo)
        {
            // must be a primary source 
            ++countPLp;
        }
    }

    Q_ASSERT(countLp<=2); // more than 2 stream readers connected ?
    Q_ASSERT(countLp<=1); // more than 1 primary source ? 

}
#endif
