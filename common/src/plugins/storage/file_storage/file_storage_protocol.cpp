#include "file_storage_protocol.h"
#include "recording/file_deletor.h"
#include "utils/common/util.h"

extern URLProtocol ufile_protocol;

void QnFileStorageResource::registerFfmpegProtocol() const
{
    av_register_protocol2(&ufile_protocol, sizeof(ufile_protocol));
}

QnFileStorageResource::QnFileStorageResource()
{
};

bool QnFileStorageResource::isNeedControlFreeSpace()
{
    return true;
}

bool QnFileStorageResource::removeFile(const QString& url)
{
    qnFileDeletor->deleteFile(removeProtocolPrefix(url));
    return true;
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
