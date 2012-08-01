#include <QDir>

#include "buffered_file.h"
#include <QSharedPointer>
#include "utils/common/util.h"
#include "core/resource/resource_fwd.h"
#include "recorder/storage_manager.h"

#ifdef Q_OS_WIN
#include "windows.h"
#endif

static const int SECTOR_SIZE = 32768;
static const qint64 AVG_USAGE_AGGREGATE_TIME = 15 * 1000000ll; // aggregation time in usecs

// -------------- QueueFileWriter ------------

QueueFileWriter::QueueFileWriter()
{
    m_writeTime = 0;
    start();
}


QueueFileWriter::~QueueFileWriter()
{
    stop();
}

qint64 QueueFileWriter::write(QBufferedFile* file, const char * data, qint64 len)
{
    if (m_needStop)
        return -1;

    FileBlockInfo fb(file, data, len);
    QMutexLocker lock(&fb.mutex);
    m_dataQueue.push(&fb);
    fb.condition.wait(&fb.mutex);
    return fb.result;
}

void QueueFileWriter::removeOldWritingStatistics(qint64 currentTime)
{
    while (!m_writeTimings.isEmpty() && m_writeTimings.front().first < currentTime - AVG_USAGE_AGGREGATE_TIME) 
    {
        m_writeTime -= m_writeTimings.front().second;
        m_writeTimings.dequeue();
    }
}

void QueueFileWriter::run()
{
    FileBlockInfo* fileBlock;
    while (!m_needStop)
    {
        if (m_dataQueue.pop(fileBlock, 100))
        {
            qint64 currentTime = getUsecTimer();
            {
                QMutexLocker lock(&fileBlock->mutex);
                fileBlock->result = fileBlock->file->writeUnbuffered(fileBlock->data, fileBlock->len);
                fileBlock->condition.wakeAll();
            }
            qint64 now = getUsecTimer();

            QMutexLocker lock(&m_timingsMutex);
            removeOldWritingStatistics(currentTime);
            m_writeTime += now - currentTime;
            m_writeTimings << WriteTimingInfo(currentTime, now - currentTime);
        }
    }

    while (m_dataQueue.pop(fileBlock, 1))
    {
        QMutexLocker lock(&fileBlock->mutex);
        fileBlock->result = -1;
        fileBlock->condition.wakeAll();
    }
}

// -------------- WriterPool ------------
QnWriterPool::QnWriterPool() 
{

}
QnWriterPool::~QnWriterPool()
{
    foreach(QueueFileWriter* writer, m_writers.values())
        delete writer;
}

QnWriterPool::WritersMap QnWriterPool::getAllWriters()
{
    QMutexLocker lock(&m_mutex);
    return m_writers;
}

QueueFileWriter* QnWriterPool::getWriter(const QString& fileName)
{
    QString drive = fileName.left(fileName.indexOf('/'));
    QnStorageResourcePtr storage = qnStorageMan->getStorageByUrl(fileName);
    if (storage)
        drive = storage->getUrl();
    if (drive.endsWith('/'))
        drive = drive.left(drive.length()-1);

    QMutexLocker lock(&m_mutex);
    WritersMap::iterator itr = m_writers.find(drive);
    if (itr == m_writers.end())
    {
        itr = m_writers.insert(drive, new QueueFileWriter());
    }
    return itr.value();
}

Q_GLOBAL_STATIC(QnWriterPool, inst)

QnWriterPool* QnWriterPool::instance()
{
    return inst();
}

// -------------- QBufferedFile -------------

QBufferedFile::QBufferedFile(const QString& fileName, int fileBlockSize, int minBufferSize): 
    m_fileEngine(fileName), 
    m_bufferSize(fileBlockSize+minBufferSize),
    m_buffer(0),
    m_queueWriter(0)
{
    m_systemDependentFlags = 0;
    m_minBufferSize = minBufferSize;
    if (m_bufferSize > 0)
        m_buffer = (quint8*) qMallocAligned(m_bufferSize, SECTOR_SIZE);
    m_bufferLen = 0;
    m_bufferPos = 0;
    m_totalWrited = 0;
    m_isDirectIO = false;
}
QBufferedFile::~QBufferedFile()
{
    close();
    qFreeAligned(m_buffer);
}
qint64    QBufferedFile::size () const
{
    if (isWritable() && m_bufferSize > 0)
        return m_totalWrited + m_bufferLen;
    else
        return m_fileEngine.size(); 
}

void QBufferedFile::disableDirectIO()
{
#ifdef Q_OS_WIN
    if (m_isDirectIO)
    {
        m_fileEngine.close();
        m_fileEngine.open(m_openMode, 0);
        m_fileEngine.seek(m_filePos);
    }
#endif
    m_isDirectIO = false;
}

qint64 QBufferedFile::writeUnbuffered(const char * data, qint64 len )
{
    return m_fileEngine.write(data, len);
}

void QBufferedFile::flushBuffer()
{
    quint8* bufferToWrite = m_buffer;
    if (m_isDirectIO) 
    {
        int toWrite = (m_bufferLen/SECTOR_SIZE)*SECTOR_SIZE;
        qint64 writed = m_queueWriter->write(this, (const char*) m_buffer, toWrite);
        if (writed == toWrite)
        {
            m_totalWrited += writed;
            m_filePos += writed;
            m_bufferLen -= writed;
            bufferToWrite += writed;
        }
    }
    disableDirectIO();
    qint64 writed = m_fileEngine.write((char*) bufferToWrite, m_bufferLen);
    if (writed > 0)
    {
        m_totalWrited += writed;
        m_filePos += writed;
    }
    m_bufferLen = 0;
    m_bufferPos = 0;

    qFreeAligned(m_buffer);
    m_buffer = 0;
    m_bufferSize = 0;
}

void QBufferedFile::close()
{
    if (m_fileEngine.isOpen())
    {
        flushBuffer();
        m_fileEngine.close();
    }
}

qint64    QBufferedFile::readData (char * data, qint64 len )
{
    qint64 rez = m_fileEngine.read(data, len);
    if (rez > 0)
        m_filePos += rez;
    return rez;
}

qint64    QBufferedFile::writeData ( const char * data, qint64 len )
{
    if (m_bufferSize == 0)
        return m_fileEngine.write((char*) data, len);

    int rez = len;
    while (len > 0)
    {
        int toWrite = qMin((int) len, m_bufferSize - m_bufferPos);
        memcpy(m_buffer + m_bufferPos, data, toWrite);
        m_bufferPos += toWrite;
        m_bufferLen = qMax(m_bufferLen, m_bufferPos);
        if (m_bufferLen == m_bufferSize) 
        {
            int writed;
            if (m_isDirectIO)
                writed = m_queueWriter->write(this, (const char*) m_buffer, m_bufferSize - m_minBufferSize);
            else
                writed = m_fileEngine.write((char*) m_buffer, m_bufferSize - m_minBufferSize);
            m_totalWrited += writed;
            if (writed !=  m_bufferSize - m_minBufferSize)
                return writed;
            m_filePos += writed;
            m_bufferLen -= writed;
            m_bufferPos -= writed;
            memmove(m_buffer, m_buffer + writed, m_bufferLen);
        }
        len -= toWrite;
        data += toWrite;
    }
    return rez;
}

bool QBufferedFile::seek(qint64 pos)
{
    if (m_bufferSize == 0) {
        m_filePos = pos;
        return m_fileEngine.seek(pos);
    }
    else 
    {
        qint64 bufferOffset = pos - (size() - m_bufferLen);
        if (bufferOffset < 0 || bufferOffset > m_bufferLen)
        {
            flushBuffer();
            m_filePos = pos;
            return m_fileEngine.seek(pos);
        }
        else {
            m_bufferPos = bufferOffset;
            return true;
        }
    }
}

bool QBufferedFile::open(QIODevice::OpenMode mode)
{
    m_openMode = mode;
    QDir dir;
    dir.mkpath(QFileInfo(m_fileEngine.getFileName()).absoluteDir().absolutePath());
    m_isDirectIO = false;
#ifdef Q_OS_WIN
    m_isDirectIO = m_systemDependentFlags & FILE_FLAG_NO_BUFFERING;
#endif
    bool rez = m_fileEngine.open(mode, m_systemDependentFlags);
    if (!rez)
        return false;
    m_filePos = 0;

    if (m_isDirectIO)
        m_queueWriter = QnWriterPool::instance()->getWriter(m_fileEngine.getFileName());

    return QIODevice::open(mode | QIODevice::Unbuffered);
}

bool QBufferedFile::isWritable() const
{
    return m_openMode & QIODevice::WriteOnly;
}

qint64    QBufferedFile::pos() const
{
    return m_filePos + m_bufferPos;
}

void QBufferedFile::setSystemFlags(int systemFlags)
{
    m_systemDependentFlags = systemFlags;
}

float QueueFileWriter::getAvarageUsage()
{
    QMutexLocker lock(&m_timingsMutex);
    removeOldWritingStatistics(getUsecTimer());
    return m_writeTime / (float) AVG_USAGE_AGGREGATE_TIME;
}
