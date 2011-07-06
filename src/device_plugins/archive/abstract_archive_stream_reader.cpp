#include "abstract_archive_stream_reader.h"

CLAbstractArchiveReader::CLAbstractArchiveReader(CLDevice* dev ) :
    CLClientPullStreamreader(dev),
    m_lengthMksec(0),
    m_startMksec(0),
    m_forward(true),
    m_singleShot(false),
    m_adaptiveSleep(20 * 1000),
    m_needToSleep(0),
    m_cs(QMutex::Recursive),
    m_useTwice(false),
    m_skipFramesToTime(0)
{
}

CLAbstractArchiveReader::~CLAbstractArchiveReader()
{
}

void CLAbstractArchiveReader::setSingleShotMode(bool single)
{
    m_singleShot = single;
    if (!m_singleShot)
    {
        m_adaptiveSleep.afterdelay();
        resume();
    }
}

bool CLAbstractArchiveReader::isSingleShotMode() const
{
    return m_singleShot;
}

void CLAbstractArchiveReader::setdirection(bool forward)
{
    m_forward = forward;
}

bool CLAbstractArchiveReader::isForwardDirection() const
{
    return m_forward;
}

// returns len of archive in mksec
quint64 CLAbstractArchiveReader::lengthMksec() const
{
    return m_lengthMksec;
}

quint64 CLAbstractArchiveReader::startMksec() const
{
    return m_startMksec;
}

void CLAbstractArchiveReader::jumpTo(quint64 mksec, bool makeshot)
{
     QMutexLocker mutex(&m_cs);

    nextFrame();

    for (int channel = 0; channel  < m_channel_number; ++channel)
        channeljumpTo(mksec, channel);

    m_useTwice = true;

    if (makeshot && isSingleShotMode())
        resume();
}

void CLAbstractArchiveReader::jumpToPreviousFrame(quint64 mksec, bool makeshot)
{
    setSkipFramesToTime(mksec);

    jumpTo(mksec - 100 * 1000, makeshot);
}

quint64 CLAbstractArchiveReader::skipFramesToTime() const
{
    QMutexLocker mutex(&m_framesMutex);

    return m_skipFramesToTime;
}

void CLAbstractArchiveReader::setSkipFramesToTime(quint64 skipFramesToTime)
{
    QMutexLocker mutex(&m_framesMutex);

    m_skipFramesToTime = skipFramesToTime;
}

bool CLAbstractArchiveReader::isSkippingFrames() const
{ 
    QMutexLocker mutex(&m_framesMutex);

    return m_skipFramesToTime != 0;
}


// ------------------- Audio tracks -------------------------

unsigned int CLAbstractArchiveReader::getCurrentAudioChannel() const
{
    return 0;
}

QStringList CLAbstractArchiveReader::getAudioTracksInfo() const
{
    return QStringList();
}

bool CLAbstractArchiveReader::setAudioChannel(unsigned int /*num*/)
{
    return false;
}
