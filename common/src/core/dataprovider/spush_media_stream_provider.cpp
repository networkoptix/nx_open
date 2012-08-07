
#include "utils/common/sleep.h"
#include "spush_media_stream_provider.h"
#include "../resource/camera_resource.h"
#include "utils/common/util.h"
#include "live_stream_provider.h"


CLServerPushStreamreader::CLServerPushStreamreader(QnResourcePtr dev ):
QnAbstractMediaStreamDataProvider(dev),
m_needReopen(false),
m_cameraAudioEnabled(false)
{
    QnPhysicalCameraResourcePtr camera = getResource().dynamicCast<QnPhysicalCameraResource>();
    if (camera) 
        m_cameraAudioEnabled = camera->isAudioEnabled();
}

bool CLServerPushStreamreader::canChangeStatus() const
{
    const QnLiveStreamProvider* liveProvider = dynamic_cast<const QnLiveStreamProvider*>(this);
    return liveProvider && liveProvider->getRole() == QnResource::Role_LiveVideo && getResource().dynamicCast<QnPhysicalCameraResource>();
}

void CLServerPushStreamreader::run()
{
    setPriority(QThread::HighPriority);
    qDebug() << "stream reader started.";

    beforeRun();

    while(!needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        if (!isStreamOpened())
        {
            openStream();
            if (!isStreamOpened())
            {
                QnSleep::msleep(100); // to avoid large CPU usage

                closeStream(); // to release resources 

                setNeedKeyData();
                mFramesLost++;
                m_stat[0].onData(0);
                m_stat[0].onEvent(CL_STAT_FRAME_LOST);

                if (mFramesLost >= MAX_LOST_FRAME) // if we lost 2 frames => connection is lost for sure (2)
                {
                    if (canChangeStatus())
                        getResource()->setStatus(QnResource::Offline);
                    m_stat[0].onLostConnection();
                }

                continue;
            }
        }

        if (m_needReopen)
        {
            m_needReopen = false;
            closeStream();
        }

        QnAbstractMediaDataPtr data = getNextData();


        if (data==0)
        {
            setNeedKeyData();
            mFramesLost++;
            m_stat[0].onData(0);
            m_stat[0].onEvent(CL_STAT_FRAME_LOST);

            if (mFramesLost == MAX_LOST_FRAME) // if we lost 2 frames => connection is lost for sure (2)
            {
                if (canChangeStatus())
                    getResource()->setStatus(QnResource::Offline);

                m_stat[0].onLostConnection();
            }

            continue;
        }

        if (getResource()->hasFlags(QnResource::local_live_cam)) // for all local live cam add MediaFlags_LIVE flag; 
            data->flags |= QnAbstractMediaData::MediaFlags_LIVE;

        checkTime(data);

        if (canChangeStatus())
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

        if (videoData)
        {
            m_stat[videoData->channelNumber].onData(data->data.size());

            if (QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(this))
                lp->onGotVideoFrame(videoData);

        }

        //qDebug() << "fps = " << m_stat[0].getFrameRate();

        // check queue sizes
        if (dataCanBeAccepted())
        {
            putData(data);
        }
        else
        {
            setNeedKeyData();
        }


    }

    if (isStreamOpened())
        closeStream();

    afterRun();

    CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader stopped."), cl_logINFO);
}

void CLServerPushStreamreader::beforeRun()
{
    QnAbstractMediaStreamDataProvider::beforeRun();
    getResource()->init();
}


void CLServerPushStreamreader::pleaseReOpen()
{
    m_needReopen = true;
}

void CLServerPushStreamreader::afterUpdate() 
{
    QnPhysicalCameraResourcePtr camera = getResource().dynamicCast<QnPhysicalCameraResource>();
    if (camera) {
        if (m_cameraAudioEnabled != camera->isAudioEnabled())
            pleaseReOpen();
        m_cameraAudioEnabled = camera->isAudioEnabled();
    }
}
