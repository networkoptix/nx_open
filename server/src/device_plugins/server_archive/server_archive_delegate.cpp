#include "server_archive_delegate.h"
#include "core/resourcemanagment/resource_pool.h"

//extern const qint64 BACKWARD_SEEK_STEP;
static const qint64 BACKWARD_SEEK_STEP =  2000 * 1000; 



QnServerArchiveDelegate::QnServerArchiveDelegate(): 
    QnAbstractArchiveDelegate(),
    m_chunkSequence(0),
    m_aviDelegate(0),
    m_reverseMode(false),
    m_selectedAudioChannel(0),
    m_opened(false)
{
    m_aviDelegate = new QnAviArchiveDelegate();
}

QnServerArchiveDelegate::~QnServerArchiveDelegate()
{
    delete m_chunkSequence;
    delete m_aviDelegate;
}

qint64 QnServerArchiveDelegate::startTime()
{
    return m_catalog ? m_catalog->minTime() : 0;
}

qint64 QnServerArchiveDelegate::endTime()
{
    return m_catalog  ? m_catalog->maxTime() : 0;
}

bool QnServerArchiveDelegate::open(QnResourcePtr resource)
{
    if (m_opened)
        return true;

    m_resource = resource;
    QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    Q_ASSERT(netResource != 0);
    m_catalog = qnStorageMan->getFileCatalog(netResource);
    m_chunkSequence = new QnChunkSequence(netResource, 0);
    m_currentChunk = m_chunkSequence->getNextChunk(m_resource);
    QString url = m_catalog->fullFileName(m_currentChunk);
    m_fileRes = QnAviResourcePtr(new QnAviResource(url));
    m_opened = m_aviDelegate->open(m_fileRes);
    if (m_opened)
        m_aviDelegate->setAudioChannel(m_selectedAudioChannel);
    return m_opened;
}

void QnServerArchiveDelegate::close()
{
    m_aviDelegate->close();
    m_catalog = DeviceFileCatalogPtr();
    delete m_chunkSequence;
    m_chunkSequence = 0;
    m_reverseMode = false;
    m_opened = false;
}

qint64 QnServerArchiveDelegate::seek(qint64 time)
{
    cl_log.log("serverArchiveDelegate. jump to ",QDateTime::fromMSecsSinceEpoch(time/1000).toString(), cl_logALWAYS);
    DeviceFileCatalog::Chunk newChunk = m_chunkSequence->getNextChunk(m_resource, time);
    if (m_currentChunk.startTime == -1)
        return -1;

    if (newChunk.startTime != m_currentChunk.startTime)
    {
        if (!switchToChunk(newChunk))
            return -1;
    }
    qint64 chunkOffset = qMax(time - m_currentChunk.startTime, 0ll);
    return m_currentChunk.startTime + m_aviDelegate->seek(chunkOffset);
}

QnAbstractMediaDataPtr QnServerArchiveDelegate::getNextData()
{
    QnAbstractMediaDataPtr data = m_aviDelegate->getNextData();
    if (!data)
    {
        DeviceFileCatalog::Chunk chunk;
        do {
            chunk = m_chunkSequence->getNextChunk(m_resource);
            if (chunk.startTime == -1)
                return QnAbstractMediaDataPtr(); // EOF
        } while (!switchToChunk(chunk));
        data = m_aviDelegate->getNextData();
    }
    if (data) {
        data->timestamp +=m_currentChunk.startTime;
    }
    return data;
}

QnVideoResourceLayout* QnServerArchiveDelegate::getVideoLayout()
{
    return m_aviDelegate->getVideoLayout();
}

QnResourceAudioLayout* QnServerArchiveDelegate::getAudioLayout()
{
    return m_aviDelegate->getAudioLayout();
}

void QnServerArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    m_reverseMode = value;
    m_aviDelegate->onReverseMode(displayTime, value);
}

AVCodecContext* QnServerArchiveDelegate::setAudioChannel(int num)
{
    m_selectedAudioChannel = num;
    return m_aviDelegate->setAudioChannel(num);
}

bool QnServerArchiveDelegate::switchToChunk(const DeviceFileCatalog::Chunk newChunk)
{
    if (newChunk.startTime == -1)
        return false;
    m_currentChunk = newChunk;
    QString url = m_catalog->fullFileName(m_currentChunk);
    m_fileRes = QnAviResourcePtr(new QnAviResource(url));
    m_aviDelegate->close();
    bool rez = m_aviDelegate->open(m_fileRes);
    if (rez)
        m_aviDelegate->setAudioChannel(m_selectedAudioChannel);
    return rez;
}
