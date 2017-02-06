#include "cpull_media_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/common/sleep.h>
#include <nx/utils/log/log.h>

#include <nx/streaming/video_data_packet.h>
#include <core/resource/camera_resource.h>


QnClientPullMediaStreamProvider::QnClientPullMediaStreamProvider(const QnResourcePtr& dev ):
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
    initSystemThreadId();
    setPriority(QThread::HighPriority);
    NX_LOG("stream reader started", cl_logDEBUG2);

    int numberOfChnnels = 1;

    const QnResourcePtr& resource = getResource();
    if (QnMediaResource* mr = dynamic_cast<QnMediaResource*>(resource.data()))
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


        if (getResource()->hasUnprocessedCommands() || // if command processor has something in the queue for this resource let it go first
            !m_resource->isInitialized())
        {
            QnSleep::msleep(5);
            continue;
        }

        const QnAbstractMediaDataPtr& data = getNextData();

        if (data==0)
        {
            setNeedKeyData();
            mFramesLost++;
            m_stat[0].onData(0, false);
            m_stat[0].onEvent(CL_STAT_FRAME_LOST);

            if (mFramesLost % MAX_LOST_FRAME == 0) // if we lost MAX_LOST_FRAME frames => connection is lost for sure 
            {
                if (canChangeStatus() && getResource()->getStatus() != Qn::Unauthorized) // avoid offline->unauthorized->offline loop
                    getResource()->setStatus(Qn::Offline);

                m_stat[0].onLostConnection();
            }

            if (!needToStop())
                QnSleep::msleep(30);

            continue;
        }

        
        
        if (getResource()->hasFlags(Qn::local_live_cam)) // for all local live cam add MediaFlags_LIVE flag; 
        {
            data->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        }
        checkTime(data);

        const QnResourcePtr& resource = getResource();
        if (dynamic_cast<QnPhysicalCameraResource*>(resource.data()))
        {
            if (getResource()->getStatus() == Qn::Unauthorized || getResource()->getStatus() == Qn::Offline)
                getResource()->setStatus(Qn::Online);
        }

        QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(data);

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
                    NX_ASSERT(false);
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
            m_stat[videoData->channelNumber].onData(
                static_cast<unsigned int>(videoData->dataSize()),
                videoData->flags & AV_PKT_FLAG_KEY);

            if (lp)
                lp->onGotVideoFrame(videoData, getLiveParams(), isCameraControlRequired());

        }

        if (data && lp && lp->getRole() == Qn::CR_SecondaryLiveVideo)
            data->flags |= QnAbstractMediaData::MediaFlags_LowQuality;


        putData(std::move(data));

        if (videoData && !isMaxFps()) {
            qint64 sleepTimeUsec = 1000*1000ll / getLiveParams().fps;
            m_fpsSleep.sleep(sleepTimeUsec / numberOfChnnels); // reduce sleep time if camera has several channels to archive requested fps per channel
        }

    }

    afterRun();

    NX_LOG("stream reader stopped", cl_logDEBUG2);
}

void QnClientPullMediaStreamProvider::beforeRun()
{
    QnAbstractMediaStreamDataProvider::beforeRun();
    getResource()->init();
}

#endif // ENABLE_DATA_PROVIDERS
