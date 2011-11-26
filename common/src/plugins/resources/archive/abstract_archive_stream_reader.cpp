#include "abstract_archive_stream_reader.h"
#include "utils/common/util.h"

QnAbstractArchiveReader::QnAbstractArchiveReader(QnResourcePtr dev ) :
    QnClientPullMediaStreamProvider(dev),
    m_lengthMksec(0),
    m_needToSleep(0),
    m_delegate(0),
    m_cycleMode(true),
    m_lastJumpTime(AV_NOPTS_VALUE)
{
}

QnAbstractArchiveReader::~QnAbstractArchiveReader()
{
    stop();
    delete m_delegate;
}

// returns len of archive in mksec
quint64 QnAbstractArchiveReader::lengthMksec() const
{
    return m_lengthMksec;
}

bool QnAbstractArchiveReader::jumpTo(qint64 mksec, bool makeshot, qint64 skipTime)
{
    bool needJump = mksec != m_lastJumpTime;
    if (needJump) 
    {
        emit beforeJump(mksec, makeshot);
        channeljumpTo(mksec, 0, skipTime);
    }
    m_lastJumpTime = mksec;

    if (makeshot && isSingleShotMode())
        CLLongRunnable::resume();
    return needJump;
}

void QnAbstractArchiveReader::jumpToPreviousFrame(qint64 mksec, bool makeshot)
{
    if (mksec != DATETIME_NOW) 
    {
        //setSkipFramesToTime(mksec);
        //jumpTo(qMax(0ll, (qint64)mksec - 200 * 1000), makeshot);
        jumpTo(qMax(0ll, mksec - 200 * 1000), makeshot, mksec);
    }
    else
        jumpTo(mksec, makeshot);
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

qint64 QnAbstractArchiveReader::startTime() const 
{ 
    //m_delegate->open(m_resource);
    return m_delegate->startTime(); 
}

qint64 QnAbstractArchiveReader::endTime() const 
{ 
    //m_delegate->open(m_resource);
    return m_delegate->endTime(); 
}

bool QnAbstractArchiveReader::open()
{
    return m_delegate ? m_delegate->open(m_resource) : false;
}

bool QnAbstractArchiveReader::isRealTimeSource() const
{
    return m_delegate->isRealTimeSource();
}

