#include "file_storage_protocol.h"
#include "recording/file_deletor.h"
#include "utils/common/util.h"

extern URLProtocol ufile_protocol;

QnFileStorageProtocol::QnFileStorageProtocol()
{
    av_register_protocol2(&ufile_protocol, sizeof(ufile_protocol));
};

URLProtocol QnFileStorageProtocol::getURLProtocol() const
{
    return ufile_protocol;
}

bool QnFileStorageProtocol::isNeedControlFreeSpace()
{
    return true;
}

bool QnFileStorageProtocol::removeFile(const QString& url)
{
    qnFileDeletor->deleteFile(removePrefix(url));
    return true;
}

bool QnFileStorageProtocol::removeDir(const QString& url)
{
    qnFileDeletor->deleteDir(removePrefix(url));
    return true;
}

bool QnFileStorageProtocol::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(removePrefix(url));
}

bool QnFileStorageProtocol::isFileExists(const QString& url)
{
    return QFile::exists(removePrefix(url));
}

qint64 QnFileStorageProtocol::getFreeSpace(const QString& url)
{
    return getDiskFreeSpace(removePrefix(url));
}

QFileInfoList QnFileStorageProtocol::getFileList(const QString& dirName)
{
    QDir dir;
    dir.cd(dirName);
    return dir.entryInfoList(QDir::Files);
}


bool QnFileStorageProtocol::isStorageAvailable(const QString& value)
{
    QString tmpDir = closeDirPath(value) + QString("tmp") + QString::number(rand());
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
