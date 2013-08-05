#include "utils/common/sleep.h"
#include "cpull_media_stream_provider.h"
#include "../resource/camera_resource.h"

QnClientPullMediaStreamProvider::QnClientPullMediaStreamProvider(QnResourcePtr dev ):
    QnLiveStreamProvider(dev),
    m_fpsSleep(100*1000)
{
}

bool QnClientPullMediaStreamProvider::canChangeStatus() const
{
    const QnLiveStreamProvider* liveProvider = dynamic_cast<const QnLiveStreamProvider*>(this);
    return liveProvider && liveProvider->canChangeStatus();
}

void QnClientPullMediaStreamProvider::run()
{
    saveSysThreadID();
    setPriority(QThread::HighPriority);
    NX_LOG("stream reader started", cl_logDEBUG1);

    int numberOfChnnels = 1;

    if (QnMediaResourcePtr mr = getResource().dynamicCast<QnMediaResource>())
    {
        numberOfChnnels = mr->getVideoLayout()->channelCount();
    }

    beforeRun();

    while(!needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        // check queue sizes

        if (!dataCanBeAccepted())
        {
            QnSleep::msleep(5);
            continue;
        }


        if (getResource()->hasUnprocessedCommands()) // if command processor has something in the queue for this resource let it go first
        {
            QnSleep::msleep(5);
            continue;
        }

        QnAbstractMediaDataPtr data = getNextData();

        if (data==0)
        {
            if (needToStop())
                continue;

            setNeedKeyData();
            mFramesLost++;
            m_stat[0].onData(0);
            m_stat[0].onEvent(CL_STAT_FRAME_LOST);

            if (mFramesLost % MAX_LOST_FRAME == 0) // if we lost MAX_LOST_FRAME frames => connection is lost for sure 
            {
                if (canChangeStatus() && getResource()->getStatus() != QnResource::Unauthorized) // avoid offline->unauthorized->offline loop
                    getResource()->setStatus(QnResource::Offline);

                m_stat[0].onLostConnection();
            }

            /*
            if (getResource()->getStatus() == QnResource::Offline) {
                for (int i = 0; i < 100 && !m_needStop; ++i)
                    QnSleep::msleep(10);
            }
            else */
            {
                QnSleep::msleep(30);
            }

            continue;
        }

        
        
        if (getResource()->hasFlags(QnResource::local_live_cam)) // for all local live cam add MediaFlags_LIVE flag; 
        {
            data->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        }
        checkTime(data);

        if (getResource().dynamicCast<QnPhysicalCameraResource>())
        {
            if (getResource()->getStatus() == QnResource::Unauthorized || getResource()->getStatus() == QnResource::Offline)
                getResource()->setStatus(QnResource::Online);
        }

        QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(data);
        

        if (mFramesLost>0) // we are alive again
        {
            if (mFramesLost >= MAX_LOST_FRAME)
            {
                m_stat[0].onEvent(CL_STAT_CAMRESETED);
            }

            mFramesLost = 0;
        }

        if (videoData && needKeyData())
        {
            // I do not like; need to do smth with it
            if (videoData->flags & AV_PKT_FLAG_KEY)
            {
                if (videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
                {
                    Q_ASSERT(false);
                    continue;
                }

                m_gotKeyFrame[videoData->channelNumber]++;
            }
            else
            {
                // need key data but got not key data
                continue;
            }
        }

        if(data)
            data->dataProvider = this;

        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(this);
        if (videoData)
        {
            m_stat[videoData->channelNumber].onData(videoData->data.size());
            if (lp)
                lp->onGotVideoFrame(videoData);
        }
        if (data && lp && lp->getRole() == QnResource::Role_SecondaryLiveVideo)
            data->flags |= QnAbstractMediaData::MediaFlags_LowQuality;


        putData(data);

        if (videoData && !isMaxFps())
            m_fpsSleep.sleep(1000*1000/getFps()/numberOfChnnels);

    }

    afterRun();

    NX_LOG("stream reader stopped", cl_logDEBUG1);
}

void QnClientPullMediaStreamProvider::beforeRun()
{
    QnAbstractMediaStreamDataProvider::beforeRun();
    getResource()->init();
}
