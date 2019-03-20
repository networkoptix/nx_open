#include "spush_media_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/common/sleep.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <nx/network/deprecated/simple_http_client.h>

#include <nx/vms/server/resource/camera.h>

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

CLServerPushStreamReader::CLServerPushStreamReader(
    const nx::vms::server::resource::CameraPtr& dev)
    :
    QnLiveStreamProvider(dev),
    m_camera(dev)
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

    bool isInitialized = m_camera->isInitialized();
    if (!isInitialized)
    {
        if (m_openStreamResult)
            m_openStreamResult = CameraDiagnostics::InitializationInProgress();
    }
    else
    {
        m_currentLiveParams = getLiveParams();
        NX_VERBOSE(this, "Openning stream with params: %1", m_currentLiveParams);
        m_openStreamResult = openStreamInternal(isControlRequired, m_currentLiveParams);
        NX_VERBOSE(this, "Open stream result: [%1]", m_openStreamResult.toString(resourcePool()));
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
        if (!needToStop())
            QnSleep::msleep(kErrorDelayTimeoutMs); //< To avoid large CPU usage.

        closeStream(); // to release resources

        setNeedKeyData();
        if (isInitialized)
            m_stat[0].onEvent(m_openStreamResult);
    }

    return m_openStreamResult;
}

void CLServerPushStreamReader::run()
{
    initSystemThreadId();
    setPriority(QThread::HighPriority);
    NX_VERBOSE(this, "Starting run loop");

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
        if (data == nullptr)
        {
            setNeedKeyData();
            m_openStreamResult = CameraDiagnostics::BadMediaStreamResult();
            m_stat[0].onEvent(m_openStreamResult);
            QnSleep::msleep(kErrorDelayTimeoutMs); //< To avoid large CPU usage
            continue;
        }

        if (m_camera->hasFlags(Qn::local_live_cam)) // for all local live cam add MediaFlags_LIVE flag;
            data->flags |= QnAbstractMediaData::MediaFlags_LIVE;

        checkAndFixTimeFromCamera(data);

        QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
        QnCompressedAudioDataPtr audioData = std::dynamic_pointer_cast<QnCompressedAudioData>(data);

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

        // check queue sizes
        if (dataCanBeAccepted())
            putData(std::move(data));
        else
            setNeedKeyData(data->channelNumber);
    }

    if (isStreamOpened())
        closeStream();

    afterRun();

    NX_VERBOSE(this, "Run loop has finished");
}

void CLServerPushStreamReader::beforeRun()
{
    QnAbstractMediaStreamDataProvider::beforeRun();
    m_camera->init();
    m_cameraAudioEnabled = m_camera->isAudioEnabled();
    connect(
        m_camera.data(),
        &QnSecurityCamResource::audioEnabledChanged,
        this,
        &CLServerPushStreamReader::at_audioEnabledChanged,
        Qt::DirectConnection);
}

void CLServerPushStreamReader::afterRun()
{
    QnAbstractMediaStreamDataProvider::afterRun();
    m_camera->disconnect(this);
}

void CLServerPushStreamReader::pleaseReopenStream()
{
    if (isRunning())
        m_needReopen = true;
}

void CLServerPushStreamReader::at_audioEnabledChanged(const QnResourcePtr& res)
{
    if (m_cameraAudioEnabled != m_camera->isAudioEnabled())
        pleaseReopenStream();
    m_cameraAudioEnabled = m_camera->isAudioEnabled();

}

CameraDiagnostics::Result CLServerPushStreamReader::lastOpenStreamResult() const
{
    return m_openStreamResult;
}

#endif // ENABLE_DATA_PROVIDERS
