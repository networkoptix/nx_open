#include "thumbnails_stream_reader.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"
#include "device_plugins/server_archive/server_archive_delegate.h"
#include "utils/common/util.h"


// used in reverse mode.
// seek by 1.5secs. It is prevents too fast seeks for short GOP, also some codecs has bagged seek function. Large step prevent seek
// forward instead seek backward
static const int MAX_KEY_FIND_INTERVAL = 10 * 1000 * 1000;

static const int FFMPEG_PROBE_BUFFER_SIZE = 1024 * 512;
static const qint64 LIVE_SEEK_OFFSET = 1000000ll * 10;

QnThumbnailsStreamReader::QnThumbnailsStreamReader(QnResourcePtr dev ) :
    QnAbstractMediaStreamDataProvider(dev)
{
    m_delegate = new QnServerArchiveDelegate();
    m_delegate->setQuality(MEDIA_Quality_Low, true);
    m_startTime = AV_NOPTS_VALUE;
    m_endTime = AV_NOPTS_VALUE;
    m_frameStep = 0;
    m_cseq = 0;
}

QnThumbnailsStreamReader::~QnThumbnailsStreamReader()
{
    delete m_delegate;
}

void QnThumbnailsStreamReader::setRange(qint64 startTime, qint64 endTime, qint64 frameStep, int cseq)
{
    QMutexLocker lock(&m_mutex);
    m_startTime = startTime;
    m_endTime = endTime;
    m_frameStep = frameStep;
    m_currentPos = AV_NOPTS_VALUE;
    m_cseq = cseq;
}

QnAbstractMediaDataPtr QnThumbnailsStreamReader::createEmptyPacket()
{
    QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
    rez->timestamp = DATETIME_NOW;
    rez->opaque = m_cseq;
    msleep(50);
    return rez;
}

QnAbstractMediaDataPtr QnThumbnailsStreamReader::getNextData()
{
    qint64 currentPos;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_runing || m_startTime == AV_NOPTS_VALUE)
            return QnAbstractMediaDataPtr();

        if (m_currentPos >= m_endTime || m_currentPos == DATETIME_NOW || m_startTime == DATETIME_NOW)
            return createEmptyPacket();

        if (m_currentPos == AV_NOPTS_VALUE)
        {
            if (m_delegate->open(m_resource))
                m_currentPos = m_startTime;
            else 
                return QnAbstractMediaDataPtr();
        }
        currentPos = m_currentPos;
    }

    currentPos = qMax(currentPos, m_delegate->seek(currentPos, true));
    QnAbstractMediaDataPtr result = m_delegate->getNextData();
    currentPos += m_frameStep;

    QMutexLocker lock(&m_mutex);
    if (m_currentPos != AV_NOPTS_VALUE)
        m_currentPos = currentPos;
    if (result) {
        result->opaque = m_cseq;
        if (qSharedPointerDynamicCast<QnEmptyMediaData>(result))
            m_currentPos = DATETIME_NOW;
    }
    else {
        m_currentPos = m_endTime;
        return createEmptyPacket();
    }
    return result;
}

void QnThumbnailsStreamReader::run()
{
    CL_LOG(cl_logINFO) cl_log.log(QLatin1String("QnThumbnailsStreamReader started."), cl_logINFO);

    beforeRun();

    while(!needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        // check queue sizes

        if (!dataCanBeAccepted())
        {
            msleep(5);
            continue;
        }

        QnAbstractMediaDataPtr data = getNextData();

        if (data==0 && !m_needStop)
        {
            setNeedKeyData();
            mFramesLost++;
            m_stat[0].onData(0);
            m_stat[0].onEvent(CL_STAT_FRAME_LOST);

            msleep(30);
            continue;
        }

        checkTime(data);

        QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(data);

        if (videoData && videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
        {
            Q_ASSERT(false);
            continue;
        }

        if (videoData && needKeyData())
        {
            if (videoData->flags & AV_PKT_FLAG_KEY)
                m_gotKeyFrame[videoData->channelNumber]++;
            else
                continue; // need key data but got not key data
        }

        if(data)
            data->dataProvider = this;

        if (videoData)
            m_stat[videoData->channelNumber].onData(videoData->data.size());


        putData(data);
    }

    afterRun();

    CL_LOG(cl_logINFO) cl_log.log(QLatin1String("QnThumbnailsStreamReader reader stopped."), cl_logINFO);
}
