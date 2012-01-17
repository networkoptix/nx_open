// This is URLPrococol to allow ffmpeg use files with non-ascii filenames

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QScopedPointer>
#include <QtCore/QSemaphore>

Q_GLOBAL_STATIC_WITH_ARGS(QSemaphore, semaphore, (4))

class SemaphoreLocker
{
public:
    inline SemaphoreLocker(QSemaphore *sem) : m_sem(sem)
    { m_sem->acquire(); }
    inline ~SemaphoreLocker()
    { m_sem->release(); }

private:
    QSemaphore *m_sem;
};


static int ufile_open(URLContext *h, const char *filename, int flags)
{
    SemaphoreLocker locker(semaphore());

    av_strstart(filename, "ufile:", &filename);

    QScopedPointer<QFile> pFile(new QFile(QString::fromUtf8(filename)));
    if (!pFile)
        return AVERROR(ENOMEM);

    QFile::OpenMode mode;

    if ((flags & AVIO_FLAG_READ) != 0 && (flags & AVIO_FLAG_WRITE) != 0)
        mode = QIODevice::ReadWrite;
    else if ((flags & AVIO_FLAG_WRITE) != 0)
        mode = QIODevice::WriteOnly;
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

    QFile *pFile = reinterpret_cast<QFile *>(h->priv_data);

    pFile->close();
    delete pFile;

    return 0;
}

static int ufile_read(URLContext *h, unsigned char *buf, int size)
{
    SemaphoreLocker locker(semaphore());

    QFile *pFile = reinterpret_cast<QFile *>(h->priv_data);

    return (int) pFile->read((char*)buf, size);
}

static int ufile_write(URLContext *h, const unsigned char *buf, int size)
{
    SemaphoreLocker locker(semaphore());

    QFile *pFile = reinterpret_cast<QFile *>(h->priv_data);

    return (int) pFile->write((const char*)buf, size);
}

static int64_t ufile_seek(URLContext *h, int64_t pos, int whence)
{
    SemaphoreLocker locker(semaphore());

    QFile *pFile = reinterpret_cast<QFile *>(h->priv_data);

    switch (whence)
    {
    case SEEK_SET: return pFile->seek(pos);
    case SEEK_CUR: return pFile->seek(pFile->pos() + pos);
    case SEEK_END: return pFile->seek(pFile->size() + pos);
    case AVSEEK_SIZE: return pFile->size();
    default: break;
    }

    return AVERROR(ENOENT);
}

URLProtocol ufile_protocol = {
    "ufile",
    ufile_open,
    ufile_read,
    ufile_write,
    ufile_seek,
    ufile_close,
};
