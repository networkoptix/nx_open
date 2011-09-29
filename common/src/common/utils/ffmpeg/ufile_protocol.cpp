// This is URLPrococol to allow ffmpeg use files with non-latin filenames

static int ufile_open(URLContext *context, const char *filename, int flags)
{
    av_strstart(filename, "ufile:", &filename);

    QFile *file = new QFile(QFile::decodeName(filename));

    QFile::OpenMode mode = QIODevice::NotOpen;
    if ((flags & AVIO_FLAG_WRITE) != 0)
        mode |= QIODevice::WriteOnly;
    if ((flags & AVIO_FLAG_READ) != 0)
        mode |= QIODevice::ReadOnly;

    if (!file->open(mode))
    {
        delete file;
        return AVERROR(ENOENT);
    }

    context->priv_data = (void *)file;

    return 0;
}

static int ufile_close(URLContext *context)
{
    QFile *f = (QFile *)context->priv_data;
    delete f;

    return 0;
}

static int64_t ufile_seek(URLContext *context, int64_t pos, int whence)
{
    QFile *file = (QFile *)context->priv_data;

    switch (whence)
    {
    case SEEK_SET: return file->seek(pos);
    case SEEK_CUR: return file->seek(file->pos() + pos);
    case SEEK_END: return file->seek(file->size() + pos);
    case AVSEEK_SIZE: return file->size();
    default: break;
    }

    return AVERROR(ENOENT);
}

static int ufile_read(URLContext *context, unsigned char *buf, int size)
{
    QFile *file = (QFile *)context->priv_data;

    return (int)file->read((char *)buf, size);
}

static URLProtocol ufile_protocol =
{
    "ufile",
    ufile_open,
    ufile_read,
    0, // ufile_write
    ufile_seek,
    ufile_close,
};
