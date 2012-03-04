#include "buffered_file.h"

#include <QSharedPointer>

static const int SECTOR_SIZE = 32768;

//QueueFileWriter QBufferedFile::m_queueWriter;


// -------------- QueueFileWriter ------------

QueueFileWriter::QueueFileWriter()
{
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

void QueueFileWriter::run()
{
    FileBlockInfo* fileBlock;
    while (!m_needStop)
    {
        if (m_dataQueue.pop(fileBlock, 100))
        {
            QMutexLocker lock(&fileBlock->mutex);
            fileBlock->result = fileBlock->file->writeUnbuffered(fileBlock->data, fileBlock->len);
            fileBlock->condition.wakeAll();
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
class WriterPool
{
public:
    WriterPool() {}
    ~WriterPool()
    {
        foreach(QueueFileWriter* writer, m_writers.values())
            delete writer;
    }

    QueueFileWriter* getWriter(const QString& fileName)
    {
        //todo: determine physical drive here by path. Now I just got storage root
        QString drive;
        int cnt = 0;
        for (int i = fileName.size()-1; i >= 0; --i)
        {
            if (fileName.at(i) == '/')
            {
                cnt++;
                if (cnt >= 7) {
                    drive = fileName.left(i);
                    break;
                }
            }
        }
        if (drive.isEmpty())
            drive = fileName.left(fileName.indexOf('/'));

        QMutexLocker lock(&m_mutex);
        WritersMap::iterator itr = m_writers.find(drive);
        if (itr == m_writers.end())
        {
            itr = m_writers.insert(drive, new QueueFileWriter());
        }
        return itr.value();
    }
private:
    QMutex m_mutex;
    typedef QMap<QString, QueueFileWriter*> WritersMap;
    WritersMap m_writers;
};


static WriterPool m_writerPool;


// -------------- QBufferedFile -------------

QBufferedFile::QBufferedFile(const QString& fileName, int fileBlockSize, int minBufferSize): 
    QnFile(fileName), 
    m_bufferSize(fileBlockSize+minBufferSize),
    m_buffer(0),
    m_queueWriter(0)
{
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
    qFreeAligned(m_buffer);
}
qint64	QBufferedFile::size () const
{
    if (isWritable() && m_bufferSize > 0)
        return m_totalWrited + m_bufferLen;
    else
        return QnFile::size(); 
}

void QBufferedFile::disableDirectIO()
{
#ifdef Q_OS_WIN
    if (m_isDirectIO)
    {
        QnFile::close();
        QnFile::open(m_openMode, 0);
        QnFile::seek(m_filePos);
    }
#endif
    m_isDirectIO = false;
}

qint64 QBufferedFile::writeUnbuffered(const char * data, qint64 len )
{
    return QnFile::write(data, len);
}

void QBufferedFile::flushBuffer()
{
    quint8* bufferToWrite = m_buffer;
    if (m_isDirectIO) 
    {
        int toWrite = (m_bufferLen/SECTOR_SIZE)*SECTOR_SIZE;
        //qint64 writed = QnFile::write((char*) m_buffer, toWrite);
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
    qint64 writed = QnFile::write((char*) bufferToWrite, m_bufferLen);
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
    flushBuffer();
    QnFile::close();
}

qint64	QBufferedFile::read (char * data, qint64 len )
{
    qint64 rez = QnFile::read(data, len);
    if (rez > 0)
        m_filePos += rez;
    return rez;
}

qint64	QBufferedFile::write ( const char * data, qint64 len )
{
    if (m_bufferSize == 0)
        return QnFile::write((char*) data, len);

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
                writed = QnFile::write((char*) m_buffer, m_bufferSize - m_minBufferSize);
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
        return QnFile::seek(pos);
    }
    else 
    {
        qint64 bufferOffset = pos - (size() - m_bufferLen);
        if (bufferOffset < 0 || bufferOffset > m_bufferLen)
        {
            flushBuffer();
            m_filePos = pos;
            return QnFile::seek(pos);
        }
        else {
            m_bufferPos = bufferOffset;
            return true;
        }
    }
}

bool QBufferedFile::open(QIODevice::OpenMode& mode, unsigned int systemDependentFlags)
{
    m_openMode = mode;
    QDir dir;
    dir.mkpath(QFileInfo(m_fileName).absoluteDir().absolutePath());
    m_isDirectIO = false;
#ifdef Q_OS_WIN
    m_isDirectIO = systemDependentFlags & FILE_FLAG_NO_BUFFERING;
#endif
    bool rez = QnFile::open(mode, systemDependentFlags);
    m_filePos = 0;

    if (m_isDirectIO)
        m_queueWriter = m_writerPool.getWriter(m_fileName);

    return rez;
}

bool QBufferedFile::isWritable() const
{
    return m_openMode & QIODevice::WriteOnly;
}

qint64	QBufferedFile::pos() const
{
    return m_filePos + m_bufferPos;
}
