#include <QDir>

#include "file_storage_resource.h"
#include "recording/file_deletor.h"
#include "utils/common/util.h"
#include "utils/common/buffered_file.h"

#ifdef Q_OS_WIN
#include "windows.h"
#endif

static const int IO_BLOCK_SIZE = 1024*1024*4;
static const int FFMPEG_BUFFER_SIZE = 1024*1024*2;

QIODevice* QnFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    QString fileName = removeProtocolPrefix(url);
    int ioBlockSize = 0;
    int ffmpegBufferSize = 0;

    int systemFlags = 0;
    if (openMode & QIODevice::WriteOnly) {
        ioBlockSize = IO_BLOCK_SIZE;
        ffmpegBufferSize = FFMPEG_BUFFER_SIZE;
#ifdef Q_OS_WIN
        systemFlags = FILE_FLAG_NO_BUFFERING;
#endif
    }
    QBufferedFile* rez = new QBufferedFile(fileName, ioBlockSize, ffmpegBufferSize);
    rez->setSystemFlags(systemFlags);
    if (!rez->open(openMode))
    {
        delete rez;
        return 0;
    }
    return rez;
}

QnFileStorageResource::QnFileStorageResource()
{

};

QnFileStorageResource::~QnFileStorageResource()
{

}

bool QnFileStorageResource::isNeedControlFreeSpace()
{
    return true;
}

bool QnFileStorageResource::removeFile(const QString& url)
{
    qnFileDeletor->deleteFile(removeProtocolPrefix(url));
    return true;
}

bool QnFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    return QFile::rename(oldName, newName);
}


bool QnFileStorageResource::removeDir(const QString& url)
{
    qnFileDeletor->deleteDir(removeProtocolPrefix(url));
    return true;
}

bool QnFileStorageResource::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(removeProtocolPrefix(url));
}

bool QnFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(removeProtocolPrefix(url));
}

qint64 QnFileStorageResource::getFreeSpace()
{
    return getDiskFreeSpace(removeProtocolPrefix(getUrl()));
}

QFileInfoList QnFileStorageResource::getFileList(const QString& dirName)
{
    QDir dir;
    dir.cd(dirName);
    return dir.entryInfoList(QDir::Files);
}

bool QnFileStorageResource::isStorageAvailableForWriting()
{
    QDir dir(getUrl());

    bool needRemoveDir = false;
    if (!dir.exists())
    {
        if (!dir.mkpath(getUrl()))
            return false;
        needRemoveDir = true;
    }

    QFile file(closeDirPath(getUrl()) + QString("tmp") + QString::number((unsigned) ((rand() << 16) + rand())));
    bool result = file.open(QFile::WriteOnly);
    if (result)
    {
        file.close();
        file.remove();
    }

    if (needRemoveDir)
        dir.remove(getUrl());

    return result;
}

bool QnFileStorageResource::isStorageAvailable()
{
    QString tmpDir = closeDirPath(getUrl()) + QString("tmp") + QString::number(rand());
    QDir dir(tmpDir);
    if (dir.exists()) {
        dir.remove(tmpDir);
        return true;
    }
    else {
        if (dir.mkpath(tmpDir))
        {
            dir.rmdir(tmpDir);
            return true;
        }
        else 
            return false;
    }

    return false;
}

int QnFileStorageResource::getChunkLen() const 
{
    return 60;
}

QString QnFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url : url.mid(prefix + 3);
}

QnStorageResource* QnFileStorageResource::instance()
{
    return new QnFileStorageResource();
}

float QnFileStorageResource::getAvarageWritingUsage() const
{
    QueueFileWriter* writer = QnWriterPool::instance()->getWriter(getUrl());
    return writer ? writer->getAvarageUsage() : 0;
}
