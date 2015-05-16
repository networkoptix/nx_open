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
                QMutexLocker lock(&fileBlock->mutex);
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

            QMutexLocker lock(&m_timingsMutex);
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
    for(QueueFileWriter* writer: m_writers.values())
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
        drive = storage->getPath();
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

Q_GLOBAL_STATIC(QnWriterPool, QnWriterPool_instance)

QnWriterPool* QnWriterPool::instance()
{
    return QnWriterPool_instance();
}

// -------------- QBufferedFile -------------

QBufferedFile::QBufferedFile(const QString& fileName, int fileBlockSize, int minBufferSize): 
    m_fileEngine(fileName), 
    m_cycleBuffer(fileBlockSize+minBufferSize, SECTOR_SIZE),
    m_queueWriter(0),
    m_cachedBuffer(CL_MEDIA_ALIGNMENT, SECTOR_SIZE),
    m_tmpBuffer(CL_MEDIA_ALIGNMENT, SECTOR_SIZE),
    m_lastSeekPos(AV_NOPTS_VALUE)
{
    m_systemDependentFlags = 0;
    m_minBufferSize = minBufferSize;
    m_bufferPos = 0;
    m_actualFileSize = 0;
    m_isDirectIO = false;
}

QBufferedFile::~QBufferedFile()
{
    close();
}
qint64 QBufferedFile::size() const
{
    if (isWritable() && m_cycleBuffer.maxSize())
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
    qint64 pos = qPower2Floor(m_cycleBuffer.size(), SECTOR_SIZE);
    qint64 toReadRest = pos + SECTOR_SIZE - m_cycleBuffer.size();
    m_fileEngine.seek(m_filePos + pos);
    if (m_fileEngine.read((char*) m_tmpBuffer.data(), SECTOR_SIZE) > 0)
        m_cycleBuffer.push_back(m_tmpBuffer.data() + (SECTOR_SIZE-toReadRest), toReadRest);
    m_fileEngine.seek(m_filePos);
}

int QBufferedFile::writeBuffer(int toWrite)
{
    int writedTotal = 0;
    int toWriteSeq = qMin(toWrite, m_cycleBuffer.seriesDataSize());
    {
        qint64 writed1 = m_queueWriter->write(this, m_cycleBuffer.data(0, toWriteSeq), toWriteSeq);
        if (writed1 > 0) {
            m_cycleBuffer.pop_front(writed1);
            writedTotal += writed1;
        }
        if (writed1 != toWriteSeq)
            return writed1; // IO error
    }
    int toWriteRest = toWrite - toWriteSeq;
    if (toWriteRest > 0)
    {
        qint64 writed2 = m_queueWriter->write(this, m_cycleBuffer.data(0, toWriteRest), toWriteRest);
        if (writed2 > 0) {
            m_cycleBuffer.pop_front(writed2);
            writedTotal += writed2;
        }
    }
    return writedTotal;
}

void QBufferedFile::flushBuffer()
{
    if (m_cycleBuffer.maxSize() == 0)
        return;

    int toWrite = m_cycleBuffer.size();
    if(m_isDirectIO) {
        toWrite = qPower2Ceil((quint32) m_cycleBuffer.size(), SECTOR_SIZE);
        if (toWrite > m_cycleBuffer.size() && m_filePos+m_cycleBuffer.size() < m_actualFileSize)
            mergeBufferWithExistingData();
        else if (toWrite > m_cycleBuffer.size()) {
            std::vector<char> fillerData;
            fillerData.resize(toWrite - m_cycleBuffer.size());
            m_cycleBuffer.push_back(fillerData.data(), fillerData.size());
        }
    }
    qint64 writed = writeBuffer(toWrite);
    if (writed > 0)
        m_filePos += writed;

    m_cycleBuffer.clear();
}

void QBufferedFile::close()
{
    if (m_fileEngine.isOpen())
    {
        int bufferingWriteSize = 0;
        if (m_cycleBuffer.size() > SECTOR_SIZE) 
        {
            // do not perform buffering write if data rest is small
            if (m_isDirectIO) {
                if (m_filePos % SECTOR_SIZE == 0)
                    bufferingWriteSize = qPower2Floor((quint32) m_cycleBuffer.size(), SECTOR_SIZE);
            }
            else {
                bufferingWriteSize = m_cycleBuffer.size(); 
            }
        }

        if (bufferingWriteSize) {
            int dataRestLen = m_cycleBuffer.size() - bufferingWriteSize;
            m_fileEngine.seek(m_filePos);
            int writed = writeBuffer(bufferingWriteSize);
            if (writed > 0)
                m_filePos += writed;
        }

        if (m_isDirectIO) {
            m_fileEngine.close();
            m_fileEngine.open(QIODevice::ReadWrite, 0);
        }

        if (m_cycleBuffer.size() > 0) {
            m_fileEngine.seek(m_filePos);
            m_fileEngine.write(m_cycleBuffer.data(), m_cycleBuffer.size());
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
    if (bufferOffset < 0 || bufferOffset > m_cycleBuffer.size())
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

    if (m_cycleBuffer.maxSize() == 0)
        return m_fileEngine.write((char*) data, len);

    int rez = len;
    while (len > 0)
    {
        if (m_cachedBuffer.size() < (uint)SECTOR_SIZE && m_cachedBuffer.size() == m_filePos + m_bufferPos)
        {
            int copyLen = qMin((int) len, (int) (SECTOR_SIZE - m_cachedBuffer.size()));
            m_cachedBuffer.write(data, copyLen);
        }

        int toWrite = qMin((int) len, m_cycleBuffer.maxSize() - m_bufferPos);
        m_cycleBuffer.insert(m_bufferPos, data, toWrite);
        m_bufferPos += toWrite;
        m_actualFileSize = qMax(m_actualFileSize, m_filePos + m_cycleBuffer.size());
        if (m_cycleBuffer.size() == m_cycleBuffer.maxSize()) 
        {
            int writed = writeBuffer(m_cycleBuffer.maxSize() - m_minBufferSize);
            if (writed > 0) {
                m_filePos += writed;
                m_bufferPos -= writed;
            }
            if (writed != m_cycleBuffer.maxSize() - m_minBufferSize)
                return writed;
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
        m_cycleBuffer.push_back(m_cachedBuffer.data(), m_cachedBuffer.size());
    }
    else {
        qint64 toRead = qPower2Ceil((quint32) bufOffset, SECTOR_SIZE);
        int readed = m_fileEngine.read(m_tmpBuffer.data(), toRead);
        if (readed == -1)
            return false;
        m_cycleBuffer.push_back(m_tmpBuffer.data(), qMin((qint64) readed, m_actualFileSize - m_filePos));
        m_fileEngine.seek(m_filePos);
    }
    m_bufferPos = bufOffset;
    return true;
}

bool QBufferedFile::seek(qint64 pos)
{
    if (m_cycleBuffer.maxSize() == 0) {
        m_filePos = pos;
        return m_fileEngine.seek(pos);
    }
    else {
        m_lastSeekPos = pos;
        return true;
    }
}


bool QBufferedFile::open(QIODevice::OpenMode mode)
{
    m_openMode = mode;
    if (mode & QIODevice::WriteOnly) {
        QDir dir;
        dir.mkpath(QnFile::absolutePath(m_fileEngine.getFileName()));
    }
    m_isDirectIO = false;
#ifdef Q_OS_WIN
    m_isDirectIO = m_systemDependentFlags & FILE_FLAG_NO_BUFFERING;
#endif
    if (m_cycleBuffer.maxSize() > 0)
        mode |= QIODevice::ReadOnly;
    bool rez = m_fileEngine.open(mode, m_systemDependentFlags);
    if (!rez)
        return false;
    m_filePos = 0;
    if (mode & QIODevice::WriteOnly)
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
    QMutexLocker lock(&m_timingsMutex);
    removeOldWritingStatistics(getUsecTimer());
    return m_writeTime / (float) AVG_USAGE_AGGREGATE_TIME;
}
