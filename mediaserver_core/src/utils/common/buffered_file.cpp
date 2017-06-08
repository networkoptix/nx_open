#include <QtCore/QDir>

extern "C"
{
    #include <libavutil/avutil.h>
}

#include <cmath>
#include "buffered_file.h"
#include <QSharedPointer>
#include "utils/common/util.h"
#include <core/resource/storage_resource.h>
#include <utils/common/writer_pool.h>
#include "recorder/storage_manager.h"
#include <nx/streaming/config.h>
#include "nx/utils/log/log.h"

#ifdef Q_OS_WIN
#include "windows.h"
#endif
#include "utils/math/math.h"

//TODO #ak make it system-dependent (it can depend on OS, FS type, FS settings)
static const int SECTOR_SIZE = 32768;
static const qint64 AVG_USAGE_AGGREGATE_TIME = 15 * 1000000ll; // aggregation time in usecs
static const int DATA_PRIORITY_THRESHOLD = SECTOR_SIZE * 2;
const int kMkvMaxHeaderOffset = 1024;

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

void QueueFileWriter::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    QnMutexLocker lock(&m_dataMutex);
    m_dataWaitCond.wakeAll();
}

qint64 QueueFileWriter::writeRanges(QBufferedFile* file, std::vector<QnMediaCyclicBuffer::Range> ranges)
{
    FileBlockInfo fb(file);
    fb.ranges = std::move(ranges);

#if 1
    QnMutexLocker lock(&fb.mutex);
    if (putData(&fb))
        fb.condition.wait(&fb.mutex);
    else
        return -1;
#else
    // use native NCQ
    for (const auto& range: fb.ranges) {
        auto writed = fb.file->writeUnbuffered(range.data, range.size);
        if (writed == range.size)
            fb.result += writed;
        else
            break;
    }
#endif
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

bool QueueFileWriter::putData(FileBlockInfo* fb)
{
    QnMutexLocker lock(&m_dataMutex);
    if (m_needStop)
        return false;

    m_dataQueue.push_back(fb);
    m_dataWaitCond.wakeAll();
    return true;
}

QueueFileWriter::FileBlockInfo* QueueFileWriter::popData()
{
    // pop block with size < DATA_PRIORITY_THRESHOLD at first

    QnMutexLocker lock(&m_dataMutex);
    while (m_dataQueue.empty() && !m_needStop)
        m_dataWaitCond.wait(&m_dataMutex, 100);

    int minBlockSize = INT_MAX;
    auto resultItr = m_dataQueue.end();
    for (auto itr = m_dataQueue.begin(); itr != m_dataQueue.end(); ++itr) {
        int dataSize = qMin(DATA_PRIORITY_THRESHOLD, (*itr)->dataSize());
        if (dataSize < minBlockSize) {
            minBlockSize = dataSize;
            resultItr = itr;
        }
    }
    if (resultItr == m_dataQueue.end())
        return 0;

    FileBlockInfo* result = *resultItr;
    m_dataQueue.erase(resultItr);
    return result;
}

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

    while (!m_needStop)
    {
        FileBlockInfo* fileBlock = popData();
        if (fileBlock)
        {
            qint64 currentTime = getUsecTimer();
            {
                QnMutexLocker lock(&fileBlock->mutex);
#ifdef MEASURE_WRITE_TIME
                writeTimeCalculationTimer.restart();
                if( !totalTimeCalculationTimer.isValid() )
                    totalTimeCalculationTimer.restart();
                //std::cout<<"Writing "<<fileBlock->len<<" bytes"<<std::endl;
#endif
                for (const auto& range: fileBlock->ranges) {
                    auto writed = fileBlock->file->writeUnbuffered(range.data, range.size);
                    if (writed == range.size)
                        fileBlock->result += writed;
                    else
                        break;
                }
#ifdef MEASURE_WRITE_TIME
                bytesWrittenInPeriod += fileBlock->result;
                //std::cout<<"Written "<<fileBlock->result<<" bytes"<<std::endl;
                totalMillisSpentWriting.fetchAndAddOrdered( writeTimeCalculationTimer.elapsed() );
#endif
                fileBlock->condition.wakeAll();
            }
            qint64 now = getUsecTimer();

            QnMutexLocker lock(&m_timingsMutex);
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

    while (FileBlockInfo* fileBlock = popData())
    {
        QnMutexLocker lock(&fileBlock->mutex);
        fileBlock->result = -1;
        fileBlock->condition.wakeAll();
    }
}

// -------------- QBufferedFile -------------

QBufferedFile::QBufferedFile(
    const std::shared_ptr<IQnFile>& fileImpl,
    int fileBlockSize,
    int minBufferSize,
    int maxBufferSize,
    const QnUuid& writerPoolId):
    m_fileEngine(fileImpl),
    m_cycleBuffer(
        qPower2Ceil((unsigned) (fileBlockSize + minBufferSize), SECTOR_SIZE),
        SECTOR_SIZE),
    m_queueWriter(0),
    m_cachedBuffer(CL_MEDIA_ALIGNMENT, SECTOR_SIZE),
    m_tmpBuffer(CL_MEDIA_ALIGNMENT, SECTOR_SIZE),
    m_lastSeekPos(AV_NOPTS_VALUE),
    m_writerPoolId(writerPoolId)
{
    m_systemDependentFlags = 0;
    m_minBufferSize = qPower2Ceil((unsigned) minBufferSize, SECTOR_SIZE);
    m_maxBufferSize = qPower2Ceil((unsigned) maxBufferSize, SECTOR_SIZE);
    m_isDirectIO = false;
    m_bufferPos = 0;
    m_actualFileSize = 0;
    m_filePos = 0;
    m_openMode = QIODevice::NotOpen;
}

QBufferedFile::~QBufferedFile()
{
    emit fileClosed((uintptr_t)this);
    close();
}
qint64 QBufferedFile::size() const
{
    if (isWritable() && m_cycleBuffer.maxSize())
        return m_actualFileSize;
    else
        return m_fileEngine->size();
}

qint64 QBufferedFile::writeUnbuffered(const char * data, qint64 len )
{
    return m_fileEngine->write(data, len);
}

void QBufferedFile::mergeBufferWithExistingData()
{
    qint64 pos = qPower2Floor(m_cycleBuffer.size(), SECTOR_SIZE);
    qint64 toReadRest = pos + SECTOR_SIZE - m_cycleBuffer.size();
    m_fileEngine->seek(m_filePos + pos);
    if (m_fileEngine->read((char*) m_tmpBuffer.data(), SECTOR_SIZE) > 0)
        m_cycleBuffer.push_back(m_tmpBuffer.data() + (SECTOR_SIZE-toReadRest), toReadRest);
    m_fileEngine->seek(m_filePos);
}

int QBufferedFile::writeBuffer(int toWrite)
{
    auto ranges = m_cycleBuffer.fragmentedData(0, toWrite);
    toWrite = 0;
    for (const auto& range: ranges)
        toWrite += range.size;

    qint64 writed = m_queueWriter->writeRanges(this, std::move(ranges));
    if (writed != toWrite)
        return writed; // IO error
    m_cycleBuffer.pop_front(toWrite);
    return toWrite;
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
            m_cycleBuffer.push_back(fillerData.data(),
                (QnMediaCyclicBuffer::size_type) fillerData.size());
        }
    }
    qint64 writed = writeBuffer(toWrite);
    if (writed > 0)
        m_filePos += writed;

    m_cycleBuffer.clear();
}

void QBufferedFile::close()
{
    if (m_fileEngine->isOpen())
    {
        if (m_cycleBuffer.size() > 0)
            flushBuffer();
#if 1
        if (m_isDirectIO) {
            m_fileEngine->close();
            m_fileEngine->open(QIODevice::WriteOnly, 0); // reopen without direct IO
            m_fileEngine->truncate(m_actualFileSize);
        }
#endif
        m_fileEngine->close();
    }
    m_lastSeekPos = AV_NOPTS_VALUE;
}

bool QBufferedFile::updatePos()
{
    qint64 bufferOffset = m_lastSeekPos - m_filePos;
    if (bufferOffset < 0 || bufferOffset > m_cycleBuffer.size())
    {
        flushBuffer();
		if (m_lastSeekPos > kMkvMaxHeaderOffset)
		{
			int fileBlockSize = m_cycleBuffer.maxSize() - m_minBufferSize;
			int curPow = std::log(m_minBufferSize / 1024) / std::log(2);
			int newBufSize = (1 << (curPow + 1)) * 1024;

            NX_LOG(lit("Seek detected for File: %1. Current FfmpegBufSize: %2. Enlarging up to %3. Delta = %4")
                    .arg((uintptr_t)this)
                    .arg(m_minBufferSize)
                    .arg(newBufSize)
                    .arg(newBufSize - m_minBufferSize),
                   cl_logDEBUG1);

			if (newBufSize < m_maxBufferSize)
            {
				m_minBufferSize = newBufSize;
				emit seekDetected(reinterpret_cast<uintptr_t>(this), newBufSize);
			}
		}
        if (m_isDirectIO)
            m_filePos = qPower2Floor((quint64) m_lastSeekPos, SECTOR_SIZE);
        else
            m_filePos = m_lastSeekPos;
        if (!m_fileEngine->seek(m_filePos))
            return false;
        m_actualFileSize = qMax(m_lastSeekPos, m_actualFileSize); // extend file on seek if need
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

    qint64 rez = m_fileEngine->read(data, len);
    if (rez > 0)
        m_filePos += rez;
    return rez;
}

bool QBufferedFile::atEnd() const
{
    return m_fileEngine->eof();
}

qint64 QBufferedFile::writeData ( const char * data, qint64 len )
{
    if (m_lastSeekPos != (qint64)AV_NOPTS_VALUE) {
        if (!updatePos())
            return -1;
    }

    if (m_cycleBuffer.maxSize() == 0)
        return m_fileEngine->write((char*) data, len);

    int rez = len;
    while (len > 0)
    {
        qint64 currentPos = m_filePos + m_bufferPos;
        if (currentPos < (uint)SECTOR_SIZE) {
            // update cached data (first sector is cached in m_cachedBuffer)
            int copyLen = qMin((int) len, (int) (SECTOR_SIZE - currentPos));
            m_cachedBuffer.writeAt(data, copyLen, currentPos);
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

bool QBufferedFile::prepareBuffer(int bufferSize)
{
    if (m_filePos == 0 && (int) m_cachedBuffer.size() > bufferSize)
    {
        m_cycleBuffer.push_back(m_cachedBuffer.data(), m_cachedBuffer.size());
    }
    else {
        qint64 toRead = qPower2Ceil((quint32) bufferSize, SECTOR_SIZE);
        int readed = 0;
        if (toRead > 0)
            readed = m_fileEngine->read(m_tmpBuffer.data(), toRead);
        if (readed == -1)
            return false;
        m_cycleBuffer.push_back(m_tmpBuffer.data(), qMin((qint64) readed, m_actualFileSize - m_filePos));
        if (m_cycleBuffer.size() < bufferSize) {
            std::vector<char> fillerData;
            fillerData.resize(bufferSize - m_cycleBuffer.size());
            m_cycleBuffer.push_back(fillerData.data(),
                (QnMediaCyclicBuffer::size_type) fillerData.size());
        }
        m_fileEngine->seek(m_filePos);
    }
    m_bufferPos = bufferSize;
    return true;
}

bool QBufferedFile::seek(qint64 pos)
{
    if (m_cycleBuffer.maxSize() == 0) {
        m_filePos = pos;
        return m_fileEngine->seek(pos);
    }
    else {
        m_lastSeekPos = pos;
        return true;
    }
}


bool QBufferedFile::open(QIODevice::OpenMode mode)
{
    m_openMode = mode;
    m_isDirectIO = false;
#ifdef Q_OS_WIN
    m_isDirectIO = m_systemDependentFlags & FILE_FLAG_NO_BUFFERING;
#endif
    if (m_cycleBuffer.maxSize() > 0)
        mode |= QIODevice::ReadOnly;
    bool rez = m_fileEngine->open(mode, m_systemDependentFlags);
    if (!rez)
        return false;
    m_filePos = 0;
    if (mode & QIODevice::WriteOnly)
        m_queueWriter = QnWriterPool::instance()->getWriter(m_writerPoolId);
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
    QnMutexLocker lock(&m_timingsMutex);
    removeOldWritingStatistics(getUsecTimer());
    return m_writeTime / (float) AVG_USAGE_AGGREGATE_TIME;
}
