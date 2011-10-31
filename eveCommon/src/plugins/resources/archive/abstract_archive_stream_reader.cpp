#include "abstract_archive_stream_reader.h"

QnAbstractArchiveReader::QnAbstractArchiveReader(QnResourcePtr dev ) :
    QnClientPullMediaStreamProvider(dev),
    m_lengthMksec(0),
    m_startMksec(0),
    m_singleShot(false),
    m_adaptiveSleep(20 * 1000),
    m_needToSleep(0),
    m_cs(QMutex::Recursive),
    m_useTwice(false),
    m_skipFramesToTime(0),
    m_delegate(0),
    m_cycleMode(true)
{
}

QnAbstractArchiveReader::~QnAbstractArchiveReader()
{
    delete m_delegate;
}

void QnAbstractArchiveReader::setSingleShotMode(bool single)
{
    m_singleShot = single;
    if (!m_singleShot)
    {
        m_adaptiveSleep.afterdelay();
        CLLongRunnable::resume();
    }
    emit singleShotModeChanged(single);
}

bool QnAbstractArchiveReader::isSingleShotMode() const
{
    return m_singleShot;
}

// returns len of archive in mksec
quint64 QnAbstractArchiveReader::lengthMksec() const
{
    return m_lengthMksec;
}

void QnAbstractArchiveReader::jumpTo(qint64 mksec, bool makeshot)
{
    emit jumpOccured(mksec, makeshot);

     //QMutexLocker mutex(&m_cs);

    //nextFrame();

    //for (int channel = 0; channel  < m_channel_number; ++channel)
        //channeljumpTo(mksec, channel);

    channeljumpTo(mksec, 0);

    m_useTwice = true;

    if (makeshot && isSingleShotMode())
        CLLongRunnable::resume();
}

void QnAbstractArchiveReader::jumpToPreviousFrame(qint64 mksec, bool makeshot)
{
    setSkipFramesToTime(mksec);

    jumpTo(qMax(0ll, (qint64)mksec - 200 * 1000), makeshot);
}

qint64 QnAbstractArchiveReader::skipFramesToTime() const
{
    QMutexLocker mutex(&m_framesMutex);

    return m_skipFramesToTime;
}

void QnAbstractArchiveReader::setSkipFramesToTime(qint64 skipFramesToTime)
{
    QMutexLocker mutex(&m_framesMutex);
    m_skipFramesToTime = skipFramesToTime;
}

bool QnAbstractArchiveReader::isSkippingFrames() const
{ 
    QMutexLocker mutex(&m_framesMutex);

    return m_skipFramesToTime != 0;
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

void QnAbstractArchiveReader::setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate)
{
    m_delegate = contextDelegate;
}

QnAbstractArchiveDelegate* QnAbstractArchiveReader::getArchiveDelegate() const
{
    return m_delegate;
}

void QnAbstractArchiveReader::setCycleMode(bool value)
{
    m_cycleMode = value;
}

void QnAbstractArchiveReader::pause()
{
    QnClientPullMediaStreamProvider::pause();
    emit streamPaused();
}

void QnAbstractArchiveReader::resume()
{
    QnClientPullMediaStreamProvider::resume();
    emit streamResumed();
}

void QnAbstractArchiveReader::nextFrame()
{
    emit nextFrameOccured();
    resume();
}

void QnAbstractArchiveReader::previousFrame(qint64 mksec)
{
    emit prevFrameOccured();
}
