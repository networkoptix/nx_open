#include <QtCore/QDir>

extern "C"
{
    #include <libavutil/avutil.h>
}

#include "buffered_file.h"
#include <QSharedPointer>
#include "utils/common/util.h"
#include <core/resource/storage_resource.h>
#include "recorder/storage_manager.h"

#ifdef Q_OS_WIN
#include "windows.h"
#endif
#include "utils/math/math.h"

//TODO #ak make it system-dependent (it can depend on OS, FS type, FS settings)
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
    SCOPED_MUTEX_LOCK( lock, &fb.mutex);
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

//#define MEASURE_WRITE_TIME

void QueueFileWriter::run()
{
#ifdef MEASURE_WRITE_TIME
    QAtomicInt totalMillisSpentWriting;
    QElapsedTimer writeTimeCalculationTimer;
    QElapsedTimer totalTimeCalculationTimer;
    totalTimeCalculationTimer.invalidate();
    int millisPassed = 0;
    size_t bytesWrittenInPeriod = 0;
#endif

    FileBlockInfo* fileBlock;
    while (!m_needStop)
    {
        if (m_dataQueue.pop(fileBlock, 100))
        {
            qint64 currentTime = getUsecTimer();
            {
                SCOPED_MUTEX_LOCK( lock, &fileBlock->mutex);
#ifdef MEASURE_WRITE_TIME
                writeTimeCalculationTimer.restart();
                if( !totalTimeCalculationTimer.isValid() )
                    totalTimeCalculationTimer.restart();
                //std::cout<<"Writing "<<fileBlock->len<<" bytes"<<std::endl;
#endif
                fileBlock->result = fileBlock->file->writeUnbuffered(fileBlock->data, fileBlock->len);
#ifdef MEASURE_WRITE_TIME
                bytesWrittenInPeriod += fileBlock->result;
                //std::cout<<"Written "<<fileBlock->result<<" bytes"<<std::endl;
                totalMillisSpentWriting.fetchAndAddOrdered( writeTimeCalculationTimer.elapsed() );
#endif
                fileBlock->condition.wakeAll();
            }
            qint64 now = getUsecTimer();

            SCOPED_MUTEX_LOCK( lock, &m_timingsMutex);
            removeOldWritingStatistics(currentTime);
            m_writeTime += now - currentTime;
            m_writeTimings << WriteTimingInfo(currentTime, now - currentTime);
        }

#ifdef MEASURE_WRITE_TIME
        if( totalTimeCalculationTimer.elapsed() - millisPassed > 10*1000 )
        {
            std::cout<<"Total writing time "<<totalMillisSpentWriting.load()<<" ms, "
                "total time "<<totalTimeCalculationTimer.elapsed()<<" ms, "<<
                "write rate "<<(bytesWrittenInPeriod/((totalTimeCalculationTimer.elapsed() - millisPassed)/1000))/1024<<" KBps "<<std::endl;
            bytesWrittenInPeriod = 0;
            millisPassed = totalTimeCalculationTimer.elapsed();
        }
#endif
    }

    while (m_dataQueue.pop(fileBlock, 1))
    {
        SCOPED_MUTEX_LOCK( lock, &fileBlock->mutex);
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
    for(QueueFileWriter* writer: m_writers.values())
        delete writer;
}

QnWriterPool::WritersMap QnWriterPool::getAllWriters()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    return m_writers;
}

QueueFileWriter* QnWriterPool::getWriter(const QString& fileName)
{
    QString drive = fileName.left(fileName.indexOf('/'));
    QnStorageResourcePtr storage = qnStorageMan->getStorageByUrl(fileName);
    if (storage)
        drive = storage->getPath();
    if (drive.endsWith('/'))
        drive = drive.left(drive.length()-1);

    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    WritersMap::iterator itr = m_writers.find(drive);
    if (itr == m_writers.end())
    {
        itr = m_writers.insert(drive, new QueueFileWriter());
    }
    return itr.value();
}

Q_GLOBAL_STATIC(QnWriterPool, QnWriterPool_instance)

QnWriterPool* QnWriterPool::instance()
{
    return QnWriterPool_instance();
}

// -------------- QBufferedFile -------------

QBufferedFile::QBufferedFile(const QString& fileName, int fileBlockSize, int minBufferSize): 
    m_fileEngine(fileName), 
    m_bufferSize(fileBlockSize+minBufferSize),
    m_buffer(0),
    m_sectorBuffer(0),
    m_queueWriter(0),
    m_cachedBuffer(CL_MEDIA_ALIGNMENT, SECTOR_SIZE),
    m_lastSeekPos(AV_NOPTS_VALUE)
{
    m_systemDependentFlags = 0;
    m_minBufferSize = minBufferSize;
    m_bufferLen = 0;
    m_bufferPos = 0;
    m_actualFileSize = 0;
    m_isDirectIO = false;

    if (m_bufferSize > 0) {
        m_buffer = (quint8*) qMallocAligned(m_bufferSize, SECTOR_SIZE);
        Q_ASSERT_X(m_buffer, Q_FUNC_INFO, "not enough memory");
        m_sectorBuffer = (quint8*) qMallocAligned(SECTOR_SIZE, SECTOR_SIZE);
    }
}

QBufferedFile::~QBufferedFile()
{
    close();
    qFreeAligned(m_buffer);
    qFreeAligned(m_sectorBuffer);
}
qint64 QBufferedFile::size() const
{
    if (isWritable() && m_bufferSize)
        return m_actualFileSize;
    else
        return m_fileEngine.size(); 
}

qint64 QBufferedFile::writeUnbuffered(const char * data, qint64 len )
{
    return m_fileEngine.write(data, len);
}

void QBufferedFile::mergeBufferWithExistingData()
{
    qint64 pos = qPower2Floor(m_bufferLen, SECTOR_SIZE);
    qint64 toReadRest = pos + SECTOR_SIZE - m_bufferLen;
    m_fileEngine.seek(m_filePos + pos);
    if (m_fileEngine.read((char*) m_sectorBuffer, SECTOR_SIZE) > 0)
        memcpy(m_buffer+m_bufferLen, m_sectorBuffer + (SECTOR_SIZE-toReadRest), toReadRest);
    m_fileEngine.seek(m_filePos);
}

void QBufferedFile::flushBuffer()
{
    if (!m_buffer)
        return;

    int toWrite = m_bufferLen;
    if(m_isDirectIO) {
        toWrite = qPower2Ceil((quint32) m_bufferLen, SECTOR_SIZE);
        if (toWrite > m_bufferLen && m_filePos+m_bufferLen < m_actualFileSize)
            mergeBufferWithExistingData();
    }
    qint64 writed = m_queueWriter->write(this, (const char*) m_buffer, toWrite);
    if (writed == toWrite)
        m_filePos += toWrite;

    m_bufferLen = 0;
    m_bufferPos = 0;
}

void QBufferedFile::close()
{
    if (m_fileEngine.isOpen())
    {
        int bufferingWriteSize = 0;
        if (m_bufferLen > SECTOR_SIZE) 
        {
            // do not perform buffering write if data rest is small
            if (m_isDirectIO) {
                if (m_filePos % SECTOR_SIZE == 0)
                    bufferingWriteSize = qPower2Floor((quint32) m_bufferLen, SECTOR_SIZE);
            }
            else {
                bufferingWriteSize = m_bufferLen; 
            }
        }

        if (bufferingWriteSize) {
            int dataRestLen = m_bufferLen - bufferingWriteSize;
            m_fileEngine.seek(m_filePos);
            m_queueWriter->write(this, (char*) m_buffer, bufferingWriteSize);
            memcpy(m_buffer, m_buffer + bufferingWriteSize, dataRestLen);
            m_filePos += bufferingWriteSize;
            m_bufferLen -= bufferingWriteSize;
        }

        if (m_isDirectIO) {
            m_fileEngine.close();
            m_fileEngine.open(QIODevice::ReadWrite, 0);
        }

        if (m_bufferLen > 0) {
            m_fileEngine.seek(m_filePos);
            m_fileEngine.write((char*) m_buffer, m_bufferLen);
        }
        
        if (m_isDirectIO)
            m_fileEngine.truncate(m_actualFileSize);
        m_fileEngine.close();
    }
    m_lastSeekPos = AV_NOPTS_VALUE;
}

bool QBufferedFile::updatePos()
{
    qint64 bufferOffset = m_lastSeekPos - m_filePos;
    if (bufferOffset < 0 || bufferOffset > m_bufferLen)
    {
        flushBuffer();
        m_filePos = qPower2Floor((quint64) m_lastSeekPos, SECTOR_SIZE);
        if (!m_fileEngine.seek(m_filePos))
            return false;
        if (!prepareBuffer(m_lastSeekPos - m_filePos))
            return false;
    }
    else {
        m_bufferPos = bufferOffset;
    }
    m_lastSeekPos = AV_NOPTS_VALUE;
    return true;
}

qint64 QBufferedFile::readData (char * data, qint64 len )
{
    if (m_lastSeekPos != (qint64)AV_NOPTS_VALUE) {
        if (!updatePos())
            return -1;
    }

    qint64 rez = m_fileEngine.read(data, len);
    if (rez > 0)
        m_filePos += rez;
    return rez;
}

qint64 QBufferedFile::writeData ( const char * data, qint64 len )
{
    if (m_lastSeekPos != (qint64)AV_NOPTS_VALUE) {
        if (!updatePos())
            return -1;
    }

    if (m_bufferSize == 0)
        return m_fileEngine.write((char*) data, len);

    int rez = len;
    while (len > 0)
    {
        if (m_cachedBuffer.size() < (uint)SECTOR_SIZE && m_cachedBuffer.size() == m_filePos + m_bufferPos)
        {
            int copyLen = qMin((int) len, (int) (SECTOR_SIZE - m_cachedBuffer.size()));
            m_cachedBuffer.write(data, copyLen);
        }

        int toWrite = qMin((int) len, m_bufferSize - m_bufferPos);
        memcpy(m_buffer + m_bufferPos, data, toWrite);
        m_bufferPos += toWrite;
        m_bufferLen = qMax(m_bufferLen, m_bufferPos);
        m_actualFileSize = qMax(m_actualFileSize, m_filePos + m_bufferLen);
        if (m_bufferLen == m_bufferSize) 
        {
            int writed;
            writed = m_queueWriter->write(this, (const char*) m_buffer, m_bufferSize - m_minBufferSize);
            if (writed !=  m_bufferSize - m_minBufferSize)
                return writed;
            m_filePos += writed;
            m_bufferLen -= writed;
            m_bufferPos -= writed;
            //TODO #ak get rid of this memmove. It is most heavy memory operation
            memmove(m_buffer, m_buffer + writed, m_bufferLen);
        }
        len -= toWrite;
        data += toWrite;
    }
    return rez;
}

bool QBufferedFile::prepareBuffer(int bufOffset)
{
    if (m_filePos == 0 && (int) m_cachedBuffer.size() > bufOffset)
    {
        memcpy(m_buffer, m_cachedBuffer.data(), m_cachedBuffer.size());
        m_bufferLen = m_cachedBuffer.size();
    }
    else {
        qint64 toRead = qPower2Ceil((quint32) bufOffset, SECTOR_SIZE);
        int readed = m_fileEngine.read((char*) m_buffer, toRead);
        if (readed == -1)
            return false;
        m_bufferLen = qMin((qint64) readed, m_actualFileSize - m_filePos);
        m_fileEngine.seek(m_filePos);
    }
    m_bufferPos = bufOffset;
    return true;
}

bool QBufferedFile::seek(qint64 pos)
{
    if (m_bufferSize == 0) {
        m_filePos = pos;
        return m_fileEngine.seek(pos);
    }
    else {
        m_lastSeekPos = pos;
        return true;
    }
}

/*
bool QBufferedFile::seek(qint64 pos)
{
    if (m_bufferSize == 0) {
        m_filePos = pos;
        return m_fileEngine.seek(pos);
    }
    else 
    {
        qint64 bufferOffset = pos - m_filePos;
        if (bufferOffset < 0 || bufferOffset > m_bufferLen)
        {
            flushBuffer();
            m_filePos = qPower2Floor((quint64) pos, SECTOR_SIZE);
            if (!m_fileEngine.seek(m_filePos))
                return false;
            prepareBuffer(pos - m_filePos);
            return true;
        }
        else {
            m_bufferPos = bufferOffset;
            return true;
        }
    }
}
*/

bool QBufferedFile::open(QIODevice::OpenMode mode)
{
    m_openMode = mode;
    QDir dir;
    dir.mkpath(QnFile::absolutePath(m_fileEngine.getFileName()));
    m_isDirectIO = false;
#ifdef Q_OS_WIN
    m_isDirectIO = m_systemDependentFlags & FILE_FLAG_NO_BUFFERING;
#endif
    if (m_bufferSize > 0)
        mode |= QIODevice::ReadOnly;
    bool rez = m_fileEngine.open(mode, m_systemDependentFlags);
    if (!rez)
        return false;
    m_filePos = 0;

    m_queueWriter = QnWriterPool::instance()->getWriter(m_fileEngine.getFileName());
    m_actualFileSize = 0;
    return QIODevice::open(mode | QIODevice::Unbuffered);
}

bool QBufferedFile::isWritable() const
{
    return m_openMode & QIODevice::WriteOnly;
}

qint64 QBufferedFile::pos() const
{
    if (m_lastSeekPos != (qint64)AV_NOPTS_VALUE)
        return m_lastSeekPos;
    else
        return m_filePos + m_bufferPos;
}

void QBufferedFile::setSystemFlags(int systemFlags)
{
    m_systemDependentFlags = systemFlags;
}

float QueueFileWriter::getAvarageUsage()
{
    SCOPED_MUTEX_LOCK( lock, &m_timingsMutex);
    removeOldWritingStatistics(getUsecTimer());
    return m_writeTime / (float) AVG_USAGE_AGGREGATE_TIME;
}
