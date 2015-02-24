#include "spush_media_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/common/sleep.h>
#include <utils/common/util.h>
#include <utils/common/log.h>
#include <utils/network/simple_http_client.h>

#include <core/resource/camera_resource.h>

static const qint64 CAM_NEED_CONTROL_CHECK_TIME = 1000 * 1;

CLServerPushStreamReader::CLServerPushStreamReader(const QnResourcePtr& dev ):
    QnLiveStreamProvider(dev),
    m_needReopen(false),
    m_cameraAudioEnabled(false),
    m_openStreamResult(CameraDiagnostics::ErrorCode::unknown),
    m_openStreamCounter(0),
    m_FrameCnt(0),
    m_cameraControlRequired(false)
{
}

CameraDiagnostics::Result CLServerPushStreamReader::diagnoseMediaStreamConnection()
{
    QMutexLocker lk( &m_openStreamMutex );

    const int openStreamCounter = m_openStreamCounter;
    while( openStreamCounter == m_openStreamCounter
        && !needToStop()
        && m_openStreamResult.errorCode != CameraDiagnostics::ErrorCode::noError )
    {
        m_cond.wait( lk.mutex() );
    }

    return m_openStreamResult;
}

bool CLServerPushStreamReader::canChangeStatus() const
{
    const QnLiveStreamProvider* liveProvider = dynamic_cast<const QnLiveStreamProvider*>(this);
    return liveProvider && liveProvider->canChangeStatus();
}

CameraDiagnostics::Result CLServerPushStreamReader::openStreamInternal()
{
    m_cameraControlRequired = QnLiveStreamProvider::isCameraControlRequired(); // cache value to prevent race condition if value will changed durinng stream opening
    return openStream();
}

bool CLServerPushStreamReader::isCameraControlRequired() const
{
    return m_cameraControlRequired;
}

void CLServerPushStreamReader::run()
{
    initSystemThreadId();
    setPriority(QThread::HighPriority);
    NX_LOG("stream reader started", cl_logDEBUG2);

    beforeRun();

    while(!needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        if (!isStreamOpened())
        {
            m_FrameCnt = 0;
            CameraDiagnostics::Result openStreamResult;
            if (!m_resource->isInitialized()) {
                openStreamResult = CameraDiagnostics::InitializationInProgress();
                if (m_openStreamResult == CameraDiagnostics::NoErrorResult())
                    m_openStreamResult = openStreamResult;
            }
            else {
                m_openStreamResult = openStreamResult = openStreamInternal();
                m_needControlTimer.restart();
            }

            {
                QMutexLocker lk( &m_openStreamMutex );
                ++m_openStreamCounter;
                m_cond.wakeAll();
            }
            if (!isStreamOpened())
            {
                QnSleep::msleep(100); // to avoid large CPU usage

                closeStream(); // to release resources 

                setNeedKeyData();

                if (openStreamResult.errorCode != CameraDiagnostics::ErrorCode::cameraInitializationInProgress)
                {
                    mFramesLost++;
                    m_stat[0].onData(0);
                    m_stat[0].onEvent(CL_STAT_FRAME_LOST);

                    if (mFramesLost >= MAX_LOST_FRAME) // if we lost 2 frames => connection is lost for sure (2)
                    {
                        if (canChangeStatus() && getResource()->getStatus() != Qn::Unauthorized) // avoid offline->unauthorized->offline loop
                            getResource()->setStatus( getLastResponseCode() == CL_HTTP_AUTH_REQUIRED ? Qn::Unauthorized : Qn::Offline);
                        m_stat[0].onLostConnection();
                        mFramesLost = 0;
                    }
                }

                continue;
            }
        }
        else {
            if (m_needControlTimer.elapsed() > CAM_NEED_CONTROL_CHECK_TIME) 
            {
                if (!m_cameraControlRequired && QnLiveStreamProvider::isCameraControlRequired())
                    pleaseReOpen();
                m_needControlTimer.restart();
            }
        }

        if (m_needReopen)
        {
            m_needReopen = false;
            closeStream();
        }

        const QnAbstractMediaDataPtr& data = getNextData();


        if (data==0)
        {
            setNeedKeyData();
            mFramesLost++;
            m_stat[0].onData(0);
            m_stat[0].onEvent(CL_STAT_FRAME_LOST);

            if (mFramesLost == MAX_LOST_FRAME) // if we lost 2 frames => connection is lost for sure (2)
            {
                if (canChangeStatus())
                    getResource()->setStatus(Qn::Offline);
                if (m_FrameCnt > 0)
                    m_resource->setLastMediaIssue(CameraDiagnostics::BadMediaStreamResult());
                else
                    m_resource->setLastMediaIssue(CameraDiagnostics::NoMediaStreamResult());
                m_stat[0].onLostConnection();
            }

            continue;
        }
        m_FrameCnt++;

        if (getResource()->hasFlags(Qn::local_live_cam)) // for all local live cam add MediaFlags_LIVE flag; 
            data->flags |= QnAbstractMediaData::MediaFlags_LIVE;

        checkTime(data);

        if (canChangeStatus())
        {
            if (getResource()->getStatus() == Qn::Unauthorized || getResource()->getStatus() == Qn::Offline)
                getResource()->setStatus(Qn::Online);
        }

        QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(data);

        if (mFramesLost>0) // we are alive again
        {
            if (mFramesLost >= MAX_LOST_FRAME)
            {
                m_stat[0].onEvent(CL_STAT_CAMRESETED);
            }
            m_resource->setLastMediaIssue(CameraDiagnostics::NoErrorResult());
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
            m_stat[videoData->channelNumber].onData(data->dataSize());
            if (lp)
                lp->onGotVideoFrame(videoData);
        }
        if (data && lp && lp->getRole() == Qn::CR_SecondaryLiveVideo)
            data->flags |= QnAbstractMediaData::MediaFlags_LowQuality;

        //qDebug() << "fps = " << m_stat[0].getFrameRate();

        // check queue sizes
        if (dataCanBeAccepted())
            putData(std::move(data));
        else
            setNeedKeyData();


    }

    if (isStreamOpened())
        closeStream();

    afterRun();

    NX_LOG("stream reader stopped", cl_logDEBUG2);
}

void CLServerPushStreamReader::beforeRun()
{
    QnAbstractMediaStreamDataProvider::beforeRun();
    getResource()->init();

    const QnPhysicalCameraResource* camera = dynamic_cast<QnPhysicalCameraResource*>(m_resource.data());
    if (camera) {
        m_cameraAudioEnabled = camera->isAudioEnabled();
        connect(camera,  SIGNAL(resourceChanged(QnResourcePtr)), this, SLOT(at_resourceChanged(QnResourcePtr)), Qt::DirectConnection);
    }
}

void CLServerPushStreamReader::afterRun()
{
    QnAbstractMediaStreamDataProvider::afterRun();
    m_resource->disconnect(this, SLOT(at_resourceChanged(QnResourcePtr)));
}

void CLServerPushStreamReader::pleaseReOpen()
{
    m_needReopen = true;
}

void CLServerPushStreamReader::at_resourceChanged(const QnResourcePtr& res)
{
    const QnPhysicalCameraResource* camera = dynamic_cast<QnPhysicalCameraResource*>(getResource().data());
    if (camera) {
        if (m_cameraAudioEnabled != camera->isAudioEnabled())
            pleaseReOpen();
        m_cameraAudioEnabled = camera->isAudioEnabled();
    }
}

#endif // ENABLE_DATA_PROVIDERS
