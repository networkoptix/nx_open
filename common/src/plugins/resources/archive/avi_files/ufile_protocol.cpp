// This is URLPrococol to allow ffmpeg use files with non-ascii filenames

static QSemaphore sem(4);


class QnSemaphoreLocker
{
public:
    inline QnSemaphoreLocker(QSemaphore *sem) : m_sem(sem)
        { m_sem->acquire(); }
    inline ~QnSemaphoreLocker()
        { m_sem->release(); }

private:
    QSemaphore *m_sem;
};

static int ufile_open(URLContext *h, const char *filename, int flags)
{
    QnSemaphoreLocker sem_locker(&sem);

    av_strstart(filename, "ufile:", &filename);

    QFile *f = new QFile(QString::fromUtf8(filename));
    if (!f)
    {
        return AVERROR(ENOMEM);
    }

    QFile::OpenMode mode;

    if ((flags & AVIO_FLAG_READ) != 0 && (flags & AVIO_FLAG_WRITE) != 0)
        mode = QIODevice::ReadWrite;
    else if ((flags & AVIO_FLAG_WRITE) != 0)
        mode = QIODevice::WriteOnly;
    else
        mode = QIODevice::ReadOnly;

    if (mode & QIODevice::WriteOnly)  {
        QDir dir;
        dir.mkpath(QFileInfo(filename).absolutePath());
    }

    if (!f->open(mode))
    {
        delete f;
        return AVERROR(ENOENT);
    }

    h->priv_data = (void *)f;

    return 0;
}

static int ufile_close(URLContext *h)
{
    QnSemaphoreLocker sem_locker(&sem);

    QFile *f = (QFile *) h->priv_data;

    if (f)
    {
        f->close();
        delete f;
    }

    return 0;
}

static int ufile_read(URLContext *h, unsigned char *buf, int size)
{
    QnSemaphoreLocker sem_locker(&sem);

    QFile* pFile = ((QFile *) h->priv_data);
    return (int) pFile->read((char*)buf, size);
}

static int ufile_write(URLContext *h, const unsigned char *buf, int size)
{
    QnSemaphoreLocker sem_locker(&sem);

    QFile* pFile = ((QFile *) h->priv_data);
    return (int) pFile->write((const char*)buf, size);
}


static int64_t ufile_seek(URLContext *h, int64_t pos, int whence)
{
    QnSemaphoreLocker sem_locker(&sem);

    QFile* pFile = ((QFile *) h->priv_data);

    switch (whence)
    {
    case SEEK_SET:
        return pFile->seek(pos);

    case SEEK_CUR:
        return pFile->seek(pFile->pos() + pos);

    case (SEEK_END):
        return pFile->seek(pFile->size() + pos);

    case (AVSEEK_SIZE):
        return pFile->size();
    }

    return AVERROR(ENOENT);
}

URLProtocol ufile_protocol =
{
    "ufile",
    ufile_open,
    ufile_read,
    ufile_write,
    ufile_seek,
    ufile_close,
};
