// This is URLPrococol to allow ffmpeg use files with non-ascii filenames

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QScopedPointer>
#include <QtCore/QSemaphore>
#include "utils/common/buffered_file.h"

static const int IO_BLOCK_SIZE = 1024*1024*4;
static const int FFMPEG_BUFFER_SIZE = 1024*1024*2;

static int ufile_open(URLContext *h, const char *filename, int flags)
{
    av_strstart(filename, "ufile:", &filename);

    QFile::OpenMode mode;
    bool useBuffer = false;

    if ((flags & AVIO_FLAG_READ) != 0 && (flags & AVIO_FLAG_WRITE) != 0)
    {
        Q_ASSERT_X(1, Q_FUNC_INFO, "Not supported");
        mode = QIODevice::ReadWrite;
    }
    else if ((flags & AVIO_FLAG_WRITE) != 0) {
        mode = QIODevice::WriteOnly | QIODevice::Unbuffered;
        useBuffer = true;
    }
    else
        mode = QIODevice::ReadOnly;

    QScopedPointer<QBufferedFile> pFile(new QBufferedFile(QString::fromUtf8(filename), useBuffer ? IO_BLOCK_SIZE : 0, useBuffer ? FFMPEG_BUFFER_SIZE : 0));
    if (!pFile)
        return AVERROR(ENOMEM);


    if (mode & QIODevice::WriteOnly) {
        QDir dir;
        dir.mkpath(QFileInfo(filename).absolutePath());
    }

    int systemFlags = 0;
#ifdef Q_OS_WIN
    if (flags & AVIO_FLAG_WRITE)
        systemFlags = FILE_FLAG_NO_BUFFERING;
#endif
    if (!pFile->open(mode, systemFlags))
        return AVERROR(ENOENT);

    h->priv_data = (void *)pFile.take();

    return 0;
}

static int ufile_close(URLContext *h)
{
    QBufferedFile *pFile = reinterpret_cast<QBufferedFile *>(h->priv_data);

    pFile->close();
    delete pFile;

    return 0;
}

static int ufile_read(URLContext *h, unsigned char *buf, int size)
{
    if (size < 0)
        return -1;
    QBufferedFile *pFile = reinterpret_cast<QBufferedFile *>(h->priv_data);

    return (int) pFile->read((char*)buf, size);
}

static int ufile_write(URLContext *h, const unsigned char *buf, int size)
{
    QBufferedFile *pFile = reinterpret_cast<QBufferedFile *>(h->priv_data);

    return (int) pFile->write((const char*)buf, size);
}

static int64_t ufile_seek(URLContext *h, int64_t pos, int whence)
{
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

    return pFile->seek(absolutePos);
}

URLProtocol ufile_protocol = {
    "ufile",
    ufile_open,
    ufile_read,
    ufile_write,
    ufile_seek,
    ufile_close,
};
