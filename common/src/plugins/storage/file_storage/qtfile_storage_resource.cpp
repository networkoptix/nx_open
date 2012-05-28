#include "qtfile_storage_resource.h"
#include "recording/file_deletor.h"
#include "utils/common/util.h"

static const int IO_BLOCK_SIZE = 1024*1024*4;
static const int FFMPEG_BUFFER_SIZE = 1024*1024*2;

QIODevice* QnQtFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    QString fileName = removeProtocolPrefix(url);

    QFile* rez = new QFile(fileName);
    if (!rez->open(openMode))
    {
        delete rez;
        return 0;
    }
    return rez;
}



QnQtFileStorageResource::QnQtFileStorageResource()
{
};

bool QnQtFileStorageResource::isNeedControlFreeSpace()
{
    return false;
}

bool QnQtFileStorageResource::removeFile(const QString& url)
{
    qnFileDeletor->deleteFile(removeProtocolPrefix(url));
    return true;
}

bool QnQtFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    return QFile::rename(oldName, newName);
}


bool QnQtFileStorageResource::removeDir(const QString& url)
{
    qnFileDeletor->deleteDir(removeProtocolPrefix(url));
    return true;
}

bool QnQtFileStorageResource::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(removeProtocolPrefix(url));
}

bool QnQtFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(removeProtocolPrefix(url));
}

qint64 QnQtFileStorageResource::getFreeSpace()
{
    return getDiskFreeSpace(removeProtocolPrefix(getUrl()));
}

QFileInfoList QnQtFileStorageResource::getFileList(const QString& dirName)
{
    QDir dir;
    dir.cd(dirName);
    return dir.entryInfoList(QDir::Files);
}


bool QnQtFileStorageResource::isStorageAvailable()
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

int QnQtFileStorageResource::getChunkLen() const 
{
    return 60;
}

QString QnQtFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url : url.mid(prefix + 3);
}

QnStorageResource* QnQtFileStorageResource::instance()
{
    return new QnQtFileStorageResource();
}

bool QnQtFileStorageResource::isStorageAvailableForWriting()
{
    return false; // it is read only file system
}
