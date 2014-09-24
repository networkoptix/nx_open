#include "abstract_archive_stream_reader.h"

#ifdef ENABLE_ARCHIVE

#include <utils/common/log.h>
#include <utils/common/util.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>


QnAbstractArchiveReader::QnAbstractArchiveReader(const QnResourcePtr& dev ) :
    QnAbstractMediaStreamDataProvider(dev),
    m_cycleMode(true),
    m_needToSleep(0),
    m_delegate(0),
    m_navDelegate(0)
{
}

QnAbstractArchiveReader::~QnAbstractArchiveReader()
{
    stop();
    delete m_delegate;
}

QnAbstractNavigator *QnAbstractArchiveReader::navDelegate() const
{
    return m_navDelegate;
}

// ------------------- Audio tracks -------------------------

unsigned int QnAbstractArchiveReader::getCurrentAudioChannel() const
{
    return 0;
}

CameraDiagnostics::Result QnAbstractArchiveReader::diagnoseMediaStreamConnection()
{
    //TODO/IMPL
    return CameraDiagnostics::Result( CameraDiagnostics::ErrorCode::unknown );
}

QStringList QnAbstractArchiveReader::getAudioTracksInfo() const
{
    return QStringList();
}

bool QnAbstractArchiveReader::setAudioChannel(unsigned int /*num*/)
{
    return false;
}

void QnAbstractArchiveReader::setNavDelegate(QnAbstractNavigator* navDelegate)
{
    m_navDelegate = navDelegate;
}

QnAbstractArchiveDelegate* QnAbstractArchiveReader::getArchiveDelegate() const
{
    return m_delegate;
}

void QnAbstractArchiveReader::setCycleMode(bool value)
{
    m_cycleMode = value;
}

bool QnAbstractArchiveReader::open()
{
    return m_delegate && m_delegate->open(m_resource);
}

void QnAbstractArchiveReader::jumpToPreviousFrame(qint64 usec)
{
    if (usec != DATETIME_NOW)
        jumpTo(qMax(0ll, usec - 200 * 1000), usec-1);
    else
        jumpTo(usec, 0);
}

quint64 QnAbstractArchiveReader::lengthUsec() const
{
    if (m_delegate)
        return m_delegate->endTime() - m_delegate->startTime();
    else
        return AV_NOPTS_VALUE;
}

void QnAbstractArchiveReader::run()
{
    initSystemThreadId();

    CL_LOG(cl_logINFO) NX_LOG(QLatin1String("QnArchiveStreamReader started."), cl_logINFO);

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

        QnAbstractMediaDataPtr data = getNextData();

        if (data==0 && !needToStop())
        {
            setNeedKeyData();
            mFramesLost++;
            m_stat[0].onData(0);
            m_stat[0].onEvent(CL_STAT_FRAME_LOST);

            QnSleep::msleep(30);
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
            m_stat[videoData->channelNumber].onData(videoData->dataSize());


        putData(std::move(data));
    }

    afterRun();

    CL_LOG(cl_logINFO) NX_LOG(QLatin1String("QnArchiveStreamReader reader stopped."), cl_logINFO);
}

#endif // ENABLE_ARCHIVE
