// This is URLPrococol to allow ffmpeg use files with non-ascii filenames

static int qtufile_open(URLContext *h, const char *filename, int flags)
{
    av_strstart(filename, "qtufile:", &filename);


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

    if (!f->open(mode))
    {
        delete f;
        return AVERROR(ENOENT);
    }

    h->priv_data = (void *)f;

    return 0;
}

static int qtufile_close(URLContext *h)
{
    QFile *f = (QFile *) h->priv_data;

    if (f)
    {
        f->close();
        delete f;
    }

    return 0;
}

static int qtufile_read(URLContext *h, unsigned char *buf, int size)
{
    QFile* pFile = ((QFile *) h->priv_data);

    return (int) pFile->read((char*)buf, size);
}

static int64_t qtufile_seek(URLContext *h, int64_t pos, int whence)
{
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

URLProtocol qtufile_protocol =
{
    "qtufile",
    qtufile_open,
    qtufile_read,
    0, // qtufile_write
    qtufile_seek,
    qtufile_close,
};
