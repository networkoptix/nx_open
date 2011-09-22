#include "base/log.h"

// This is URLPrococol to allow ffmpeg use files with non-ascii filenames

const static int UFILE_MIN_BUFFER_SIZE = 1024*64;
const static int UFILE_MAX_BUFFER_SIZE = 1024*1024;

struct UFileData
{
    UFileData(const QString& fileName): file(fileName), bufSize(0), head(0), maxBufferSize(UFILE_MIN_BUFFER_SIZE)
    {

    }

    QFile file;
    quint8 buffer[UFILE_MAX_BUFFER_SIZE];
    int bufSize;
    int head;
    int maxBufferSize;
};

static int ufile_open(URLContext *h, const char *filename, int flags)
{
    av_strstart(filename, "ufile:", &filename);

    UFileData *f = new UFileData(QString::fromUtf8(filename));
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
    mode |= QIODevice::Unbuffered;
    if (!f->file.open(mode))
    {
        delete f;
        return AVERROR(ENOENT);
    }
    h->priv_data = (void *)f;

    return 0;
}

static int ufile_close(URLContext *h)
{
    UFileData *f = (UFileData *) h->priv_data;

    if (f)
    {
        f->file.close();
        delete f;
    }

    return 0;
}

static int ufile_read(URLContext *h, unsigned char *buf, int size)
{
    UFileData* f = ((UFileData *) h->priv_data);
    //return f->file.read((char*) buf, size);

    if (f->bufSize < size) 
    {
        if (f->head)
            memmove(f->buffer, f->buffer + f->head, f->bufSize);
        f->head = 0;
        int readed = f->file.read((char*)(f->buffer + f->bufSize), f->maxBufferSize - f->bufSize);
        if (readed > 0) 
            f->bufSize += readed;
        if (f->maxBufferSize < UFILE_MAX_BUFFER_SIZE)
            f->maxBufferSize = qMin(UFILE_MAX_BUFFER_SIZE, f->maxBufferSize*2);
    }
    int rez = qMin(f->bufSize, size);
    memcpy(buf, f->buffer + f->head, rez);
    f->head += rez;
    f->bufSize -= rez;
    return rez;
}

static int64_t ufile_seek(URLContext *h, int64_t pos, int whence)
{
    UFileData* f = ((UFileData *) h->priv_data);
    qint64 newPos;
    switch (whence)
    {
    case SEEK_SET:
        newPos = pos;
        break;
    case SEEK_CUR:
        newPos = f->file.pos()-f->bufSize + pos;

    case (SEEK_END):
        newPos = f->file.size() + pos;

    case (AVSEEK_SIZE):
        return f->file.size();
    default:
        return AVERROR(ENOENT);
    }
    qint64 delta = newPos - f->file.pos();
    if (delta >= 0 && delta < f->bufSize) {
        f->bufSize -= delta;
        f->head += delta;
    }
    else 
    {
        f->bufSize = f->head = 0;
        f->file.seek(newPos);
        f->maxBufferSize = UFILE_MIN_BUFFER_SIZE;
    }
    return f->file.pos();
}

URLProtocol ufile_protocol =
{
    "ufile",
    ufile_open,
    ufile_read,
    0, // ufile_write
    ufile_seek,
    ufile_close,
};
