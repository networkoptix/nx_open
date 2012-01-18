// This is URLPrococol to allow ffmpeg use files with non-ascii filenames

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QScopedPointer>
#include <QtCore/QSemaphore>

Q_GLOBAL_STATIC_WITH_ARGS(QSemaphore, semaphore, (4))

class QBufferedFile: public QFile
{
public:
    QBufferedFile(const QString& fileName, int bufferSize = 1024*1024*2): QFile(fileName), m_bufferSize(bufferSize) 
    {
        m_writeBuffer = new quint8[bufferSize];
        m_writeBufferLen = 0;
        m_writeBufferPos = 0;
    }
    virtual ~QBufferedFile()
    {
        delete [] m_writeBuffer;
    }
    virtual qint64	size () const
    {
        return QFile::size() + m_writeBufferLen;
    }

    virtual qint64	pos() const
    {
        return QFile::pos() + m_writeBufferLen;
    }

    void flushBuffer()
    {
        QFile::writeData((char*) m_writeBuffer, m_writeBufferLen);
        m_writeBufferLen = 0;
        m_writeBufferPos = 0;
    }

    virtual void close()
    {
        flushBuffer();
        QFile::close();
    }

protected:
    virtual qint64	writeData ( const char * data, qint64 len )
    {
        int rez = len;
        while (len > 0)
        {
            int toWrite = qMin((int) len, m_bufferSize - m_writeBufferPos);
            memcpy(m_writeBuffer + m_writeBufferPos, data, toWrite);
            m_writeBufferPos += toWrite;
            m_writeBufferLen = qMax(m_writeBufferLen, m_writeBufferPos);
            if (m_writeBufferLen == m_bufferSize) {
                int writed = QFile::writeData((char*) m_writeBuffer, m_bufferSize/2);
                if (writed !=  m_bufferSize/2)
                    return writed;
                m_writeBufferLen -= writed;
                m_writeBufferPos -= writed;
                memmove(m_writeBuffer, m_writeBuffer + writed, m_writeBufferLen);
            }
            len -= toWrite;
            data += toWrite;
        }
        return rez;
    }
private:
    int m_bufferSize;
    quint8* m_writeBuffer;
    qint64 m_totalWrited;
public:
    int m_writeBufferLen;
    int m_writeBufferPos;
};

class SemaphoreLocker
{
public:
    inline SemaphoreLocker(QSemaphore *sem) : m_sem(sem)
    { if (m_sem) m_sem->acquire(); }
    inline ~SemaphoreLocker()
    { if (m_sem) m_sem->release(); }

private:
    QSemaphore *m_sem;
};


static int ufile_open(URLContext *h, const char *filename, int flags)
{
    SemaphoreLocker locker(semaphore());

    av_strstart(filename, "ufile:", &filename);

    QScopedPointer<QBufferedFile> pFile(new QBufferedFile(QString::fromUtf8(filename)));
    if (!pFile)
        return AVERROR(ENOMEM);

    QFile::OpenMode mode;

    if ((flags & AVIO_FLAG_READ) != 0 && (flags & AVIO_FLAG_WRITE) != 0)
        mode = QIODevice::ReadWrite;
    else if ((flags & AVIO_FLAG_WRITE) != 0)
        mode = QIODevice::WriteOnly | QIODevice::Unbuffered;
    else
        mode = QIODevice::ReadOnly;

    if (mode & QIODevice::WriteOnly) {
        QDir dir;
        dir.mkpath(QFileInfo(filename).absolutePath());
    }

    if (!pFile->open(mode))
        return AVERROR(ENOENT);

    h->priv_data = (void *)pFile.take();

    return 0;
}

static int ufile_close(URLContext *h)
{
    SemaphoreLocker locker(semaphore());

    QBufferedFile *pFile = reinterpret_cast<QBufferedFile *>(h->priv_data);

    pFile->close();
    delete pFile;

    return 0;
}

static int ufile_read(URLContext *h, unsigned char *buf, int size)
{
    SemaphoreLocker locker(semaphore());

    QBufferedFile *pFile = reinterpret_cast<QBufferedFile *>(h->priv_data);

    Q_ASSERT(!pFile->isWritable());

    return (int) pFile->read((char*)buf, size);
}

static int ufile_write(URLContext *h, const unsigned char *buf, int size)
{
    SemaphoreLocker locker(semaphore());

    QBufferedFile *pFile = reinterpret_cast<QBufferedFile *>(h->priv_data);

    return (int) pFile->write((const char*)buf, size);
}

static int64_t ufile_seek(URLContext *h, int64_t pos, int whence)
{
    SemaphoreLocker locker(semaphore());

    QBufferedFile *pFile = reinterpret_cast<QBufferedFile *>(h->priv_data);

    if (whence == AVSEEK_SIZE)
        return pFile->size();

    qint64 absolutePos = pos;
    switch (whence)
    {
        case SEEK_SET: 
            break;
        case SEEK_CUR: 
            absolutePos = pFile->pos() + pos;
            break;
        case SEEK_END: 
            absolutePos = pFile->size() + pos;
            break;
        default:
            return AVERROR(ENOENT);
    }
    absolutePos = qMin(pFile->size(), absolutePos);
    qint64 bufferOffset = absolutePos - (pFile->size() - pFile->m_writeBufferLen);
    if (bufferOffset < 0 || bufferOffset > pFile->m_writeBufferLen)
    {
        pFile->flushBuffer();
        return pFile->seek(absolutePos);
    }
    else {
        pFile->m_writeBufferPos = bufferOffset;
        return absolutePos;
    }
}

URLProtocol ufile_protocol = {
    "ufile",
    ufile_open,
    ufile_read,
    ufile_write,
    ufile_seek,
    ufile_close,
};
