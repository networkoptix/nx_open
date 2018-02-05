#include "spush_media_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/common/sleep.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <nx/network/deprecated/simple_http_client.h>

#include <core/resource/camera_resource.h>
#include <nx/fusion//model_functions.h>

#include "mediaserver_ini.h"

namespace {
static const qint64 CAM_NEED_CONTROL_CHECK_TIME = 1000 * 1;
static const int kErrorDelayTimeoutMs = 100;
} // namespace

static QnAbstractMediaDataPtr createMetadataPacket()
{
    QnAbstractMediaDataPtr rez = QnCompressedMetadata::createMediaEventPacket(
        DATETIME_NOW,
        Qn::MediaStreamEvent::TooManyOpenedConnections);
    rez->flags |= QnAbstractMediaData::MediaFlags_LIVE;
    QnSleep::msleep(50);
    return rez;
}


CLServerPushStreamReader::CLServerPushStreamReader(const QnResourcePtr& dev ):
    QnLiveStreamProvider(dev),
    m_needReopen(false),
    m_cameraAudioEnabled(false),
    m_openStreamResult(CameraDiagnostics::ErrorCode::unknown),
    m_openStreamCounter(0),
    m_FrameCnt(0),
    m_openedWithStreamCtrl(false)
{
}

CameraDiagnostics::Result CLServerPushStreamReader::diagnoseMediaStreamConnection()
{
    QnMutexLocker lk( &m_openStreamMutex );

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

CameraDiagnostics::Result CLServerPushStreamReader::openStream()
{
    return openStreamWithErrChecking(isCameraControlRequired());
}

bool CLServerPushStreamReader::isCameraControlRequired() const
{
    return !isCameraControlDisabled() && needConfigureProvider();
}

bool CLServerPushStreamReader::processOpenStreamResult()
{
    if (m_openStreamResult.errorCode == CameraDiagnostics::ErrorCode::tooManyOpenedConnections)
    {
        const QnAbstractMediaDataPtr& data = createMetadataPacket();
        if (dataCanBeAccepted())
            putData(std::move(data));

        return false;
    }

    return true;
}

CameraDiagnostics::Result CLServerPushStreamReader::openStreamWithErrChecking(bool isControlRequired)
{
    onStreamReopen();
    m_FrameCnt = 0;
    bool isInitialized = m_resource->isInitialized();
    if (!isInitialized)
    {
        if (m_openStreamResult)
            m_openStreamResult = CameraDiagnostics::InitializationInProgress();
    }
    else
    {
        m_currentLiveParams = getLiveParams();
        m_openStreamResult = openStreamInternal(isControlRequired, m_currentLiveParams);
        m_needControlTimer.restart();
        m_openedWithStreamCtrl = isControlRequired;
    }

    {
        QnMutexLocker lk( &m_openStreamMutex );
        ++m_openStreamCounter;
        m_cond.wakeAll();
    }

    if (!isStreamOpened())
    {
        QnSleep::msleep(kErrorDelayTimeoutMs); // to avoid large CPU usage

        closeStream(); // to release resources

        setNeedKeyData();
        if (isInitialized)
		{
            mFramesLost++;
            m_stat[0].onData(0, false);
            m_stat[0].onEvent(CL_STAT_FRAME_LOST);

            if (mFramesLost >= MAX_LOST_FRAME) // if we lost 2 frames => connection is lost for sure (2)
            {
                if (canChangeStatus() && getResource()->getStatus() != Qn::Unauthorized) // avoid offline->unauthorized->offline loop
                    getResource()->setStatus( getLastResponseCode() == CL_HTTP_AUTH_REQUIRED ? Qn::Unauthorized : Qn::Offline);
                m_stat[0].onLostConnection();
                mFramesLost = 0;
            }
        }
    }

    return m_openStreamResult;
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
            openStream();
            processOpenStreamResult();
            continue;
        }
        else if (m_needControlTimer.elapsed() > CAM_NEED_CONTROL_CHECK_TIME)
        {
            m_needControlTimer.restart();
            if (!m_openedWithStreamCtrl && isCameraControlRequired()) {
                closeStream();
                openStreamWithErrChecking(true);
                continue;
            }
        }

        if (m_needReopen)
        {
            m_needReopen = false;
            closeStream();
            continue;
        }

        if (!processOpenStreamResult())
            continue;

        const QnAbstractMediaDataPtr& data = getNextData();
        if (data==0)
        {
            setNeedKeyData();
            mFramesLost++;
            m_stat[0].onData(0, false);
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
            if (mFramesLost > MAX_LOST_FRAME)
                QnSleep::msleep(kErrorDelayTimeoutMs); // to avoid large CPU usage
            continue;
        }
        m_FrameCnt++;

        if (getResource()->hasFlags(Qn::local_live_cam)) // for all local live cam add MediaFlags_LIVE flag;
            data->flags |= QnAbstractMediaData::MediaFlags_LIVE;

        checkAndFixTimeFromCamera(data);

        if (canChangeStatus())
        {
            if (getResource()->getStatus() == Qn::Unauthorized || getResource()->getStatus() == Qn::Offline)
                getResource()->setStatus(Qn::Online);
        }

        QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
        QnCompressedAudioDataPtr audioData = std::dynamic_pointer_cast<QnCompressedAudioData>(data);

        if (mFramesLost>0) // we are alive again
        {
            if (mFramesLost >= MAX_LOST_FRAME)
            {
                m_stat[0].onEvent(CL_STAT_CAMRESETED);
            }
            m_resource->setLastMediaIssue(CameraDiagnostics::NoErrorResult());
            mFramesLost = 0;
        }

        if (videoData && needKeyData(videoData->channelNumber))
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
                static_cast<unsigned int>(data->dataSize()),
                videoData->flags & AV_PKT_FLAG_KEY);

            if (lp)
                lp->onGotVideoFrame(videoData, m_currentLiveParams, m_openedWithStreamCtrl);
        }
        else
        if (audioData)
        {
            if (lp)
                lp->onGotAudioFrame(audioData);
        }

        if (data && lp && lp->getRole() == Qn::CR_SecondaryLiveVideo)
            data->flags |= QnAbstractMediaData::MediaFlags_LowQuality;

        //qDebug() << "fps = " << m_stat[0].getFrameRate();

        // check queue sizes
        if (dataCanBeAccepted())
            putData(std::move(data));
        else
            setNeedKeyData(data->channelNumber);
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

    if (QnSecurityCamResourcePtr camera = m_resource.dynamicCast<QnSecurityCamResource>()) {
        m_cameraAudioEnabled = camera->isAudioEnabled();
        // TODO: #GDM get rid of resourceChanged
        connect(camera.data(),  &QnResource::resourceChanged, this,
            &CLServerPushStreamReader::at_resourceChanged, Qt::DirectConnection);
    }
}

void CLServerPushStreamReader::afterRun()
{
    QnAbstractMediaStreamDataProvider::afterRun();
    m_resource->disconnect(this, SLOT(at_resourceChanged(QnResourcePtr)));
}

void CLServerPushStreamReader::pleaseReopenStream()
{
    if (isRunning())
        m_needReopen = true;
}

void CLServerPushStreamReader::at_resourceChanged(const QnResourcePtr& res)
{
    NX_ASSERT(res == getResource(), Q_FUNC_INFO, "Make sure we are listening to correct resource");
    if (QnSecurityCamResourcePtr camera = res.dynamicCast<QnSecurityCamResource>()) {
        if (m_cameraAudioEnabled != camera->isAudioEnabled())
            pleaseReopenStream();
        m_cameraAudioEnabled = camera->isAudioEnabled();
    }
}

#endif // ENABLE_DATA_PROVIDERS
