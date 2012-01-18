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
        m_buffer = new quint8[bufferSize];
        m_bufferLen = 0;
        m_bufferPos = 0;
        m_totalWrited = 0;
    }
    virtual ~QBufferedFile()
    {
        delete [] m_buffer;
    }
    virtual qint64	size () const
    {
        if (isWritable())
            return m_totalWrited + m_bufferLen;
        else
            return QFile::size(); 
    }

    virtual qint64	pos() const
    {
        return QFile::pos() + m_bufferPos;
    }

    void flushBuffer()
    {
        m_totalWrited += QFile::writeData((char*) m_buffer, m_bufferLen);
        m_bufferLen = 0;
        m_bufferPos = 0;
    }

    virtual void close()
    {
        flushBuffer();
        QFile::close();
    }

protected:
    virtual qint64	writeData ( const char * data, qint64 len )
    {
        //return QFile::writeData(data, len);

        int rez = len;
        while (len > 0)
        {
            int toWrite = qMin((int) len, m_bufferSize - m_bufferPos);
            memcpy(m_buffer + m_bufferPos, data, toWrite);
            m_bufferPos += toWrite;
            m_bufferLen = qMax(m_bufferLen, m_bufferPos);
            if (m_bufferLen == m_bufferSize) {
                int writed = QFile::writeData((char*) m_buffer, m_bufferSize/2);
                m_totalWrited += writed;
                if (writed !=  m_bufferSize/2)
                    return writed;
                m_bufferLen -= writed;
                m_bufferPos -= writed;
                memmove(m_buffer, m_buffer + writed, m_bufferLen);
            }
            len -= toWrite;
            data += toWrite;
        }
        return rez;
    }
private:
    int m_bufferSize;
    quint8* m_buffer;
public:
    int m_bufferLen;
    int m_bufferPos;
    qint64 m_totalWrited;
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
    {
        Q_ASSERT_X(1, Q_FUNC_INFO, "Not supported");
        mode = QIODevice::ReadWrite;
    }
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
    qint64 bufferOffset = absolutePos - (pFile->size() - pFile->m_bufferLen);
    if (bufferOffset < 0 || bufferOffset > pFile->m_bufferLen)
    {
        pFile->flushBuffer();
        return pFile->seek(absolutePos);
    }
    else {
        pFile->m_bufferPos = bufferOffset;
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
