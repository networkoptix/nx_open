#include "thumbnails_stream_reader.h"

#include <utils/common/log.h>
#include <utils/common/util.h>

#include <core/resource/security_cam_resource.h>

#include "plugins/resource/avi/avi_archive_delegate.h"
#include "plugins/resource/avi/thumbnails_archive_delegate.h"


// used in reverse mode.
// seek by 1.5secs. It is prevents too fast seeks for short GOP, also some codecs has bagged seek function. Large step prevent seek
// forward instead seek backward
static const int MAX_KEY_FIND_INTERVAL = 10 * 1000 * 1000;

static const int FFMPEG_PROBE_BUFFER_SIZE = 1024 * 512;
static const qint64 LIVE_SEEK_OFFSET = 1000000ll * 10;

QnThumbnailsStreamReader::QnThumbnailsStreamReader(const QnResourcePtr& dev )
:
    QnAbstractMediaStreamDataProvider(dev)
{
    QnSecurityCamResource* camRes = dynamic_cast<QnSecurityCamResource*>(dev.data());
    if (camRes)
        m_archiveDelegate = camRes->createArchiveDelegate();
    if (!m_archiveDelegate)
        m_archiveDelegate = new QnServerArchiveDelegate(); // default value

    m_archiveDelegate->setQuality(MEDIA_Quality_Low, true);
    m_delegate = new QnThumbnailsArchiveDelegate(QnAbstractArchiveDelegatePtr(m_archiveDelegate));
    m_cseq = 0;
}

void QnThumbnailsStreamReader::setQuality(MediaQuality q)
{
    m_archiveDelegate->setQuality(q, true);
}


QnThumbnailsStreamReader::~QnThumbnailsStreamReader()
{
    stop();
    delete m_delegate;
}

void QnThumbnailsStreamReader::setRange(qint64 startTime, qint64 endTime, qint64 frameStep, int cseq)
{
    QMutexLocker lock(&m_mutex);
    m_delegate->setRange(startTime, endTime, frameStep);
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
    QnAbstractMediaDataPtr result = m_delegate->getNextData();
    if (result) 
        return result;
    else 
        return createEmptyPacket();
}

void QnThumbnailsStreamReader::run()
{
    CL_LOG(cl_logINFO) NX_LOG(QLatin1String("QnThumbnailsStreamReader started."), cl_logINFO);

    beforeRun();

    m_delegate->open(getResource());

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

        if(data) {
            data->dataProvider = this;
            data->opaque = m_cseq;
        }

        if (videoData)
            m_stat[videoData->channelNumber].onData(videoData->dataSize());


        putData(data);
    }

    afterRun();

    CL_LOG(cl_logINFO) NX_LOG(QLatin1String("QnThumbnailsStreamReader reader stopped."), cl_logINFO);
}

void QnThumbnailsStreamReader::afterRun()
{
    if (m_delegate)
        m_delegate->close();
}

void QnThumbnailsStreamReader::setGroupId(const QByteArray& groupId)
{
    m_delegate->setGroupId(groupId);
}
