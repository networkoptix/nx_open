#include "abstract_archive_stream_reader.h"

#include "utils/common/util.h"

QnAbstractArchiveReader::QnAbstractArchiveReader(QnResourcePtr dev ) :
    QnClientPullMediaStreamProvider(dev),
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

void QnAbstractArchiveReader::setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate)
{
    m_delegate = contextDelegate;
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
