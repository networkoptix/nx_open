#include "abstract_archive_stream_reader.h"

#include "utils/common/util.h"

QnAbstractArchiveReader::QnAbstractArchiveReader(QnResourcePtr dev ) :
    QnAbstractMediaStreamDataProvider(dev),
    m_needToSleep(0),
    m_delegate(0),
    m_navDelegate(0),
    m_cycleMode(true)
{
}

QnAbstractArchiveReader::~QnAbstractArchiveReader()
{
    stop();
    delete m_delegate;
}

// ------------------- Audio tracks -------------------------

unsigned int QnAbstractArchiveReader::getCurrentAudioChannel() const
{
    return 0;
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

qint64 QnAbstractArchiveReader::startTime() const
{
    Q_ASSERT(m_delegate);
    //m_delegate->open(m_resource);
    return m_delegate->startTime();
}

qint64 QnAbstractArchiveReader::endTime() const
{
    Q_ASSERT(m_delegate);
    //m_delegate->open(m_resource);
    return m_delegate->endTime();
}

bool QnAbstractArchiveReader::open()
{
    return m_delegate && m_delegate->open(m_resource);
}

bool QnAbstractArchiveReader::isRealTimeSource() const
{
    return m_delegate && m_delegate->isRealTimeSource();
}

void QnAbstractArchiveReader::jumpToPreviousFrame(qint64 mksec)
{
    if (mksec != DATETIME_NOW)
        jumpTo(qMax(0ll, mksec - 200 * 1000), mksec);
    else
        jumpTo(mksec, 0);
}

quint64 QnAbstractArchiveReader::lengthMksec() const
{
    if (m_delegate)
        return m_delegate->endTime() - m_delegate->startTime();
    else
        return AV_NOPTS_VALUE;
}

void QnAbstractArchiveReader::run()
{
    CL_LOG(cl_logINFO) cl_log.log(QLatin1String("QnArchiveStreamReader started."), cl_logINFO);

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

        if (data==0 && !m_needStop)
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
            m_stat[videoData->channelNumber].onData(videoData->data.size());


        putData(data);
    }

    afterRun();

    CL_LOG(cl_logINFO) cl_log.log(QLatin1String("QnArchiveStreamReader reader stopped."), cl_logINFO);
}
