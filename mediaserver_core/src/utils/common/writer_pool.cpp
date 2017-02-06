#include "writer_pool.h"
#include <utils/common/buffered_file.h>

// -------------- WriterPool ------------
QnWriterPool::QnWriterPool(QObject *parent) : Connective<QObject>(parent)
{

}

QnWriterPool::~QnWriterPool()
{
    for(QueueFileWriter* writer: m_writers.values())
        delete writer;
}

QnWriterPool::WritersMap QnWriterPool::getAllWriters()
{
    QnMutexLocker lock(&m_mutex);
    return m_writers;
}

QueueFileWriter* QnWriterPool::getWriter(const QnUuid& writerPoolId)
{

    QnMutexLocker lock(&m_mutex);
    WritersMap::iterator itr = m_writers.find(writerPoolId);
    if (itr == m_writers.end())
        itr = m_writers.insert(writerPoolId, new QueueFileWriter());
    NX_ASSERT(m_writers.size() < 16); // increase this value if you need more storages
    return itr.value();
}
