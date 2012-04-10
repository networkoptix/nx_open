#include "coldstore_storage.h"

QnPlColdStoreStorage::QnPlColdStoreStorage()
{

}

void QnPlColdStoreStorage::registerFfmpegProtocol() const
{
    // todo: rigeseter ffmpeg protocol here
}

int QnPlColdStoreStorage::getChunkLen() const 
{
    return 60*60;
}

bool QnPlColdStoreStorage::isStorageAvailable() 
{
    return true;
}

QFileInfoList QnPlColdStoreStorage::getFileList(const QString& dirName) 
{
    return QFileInfoList();
}

bool QnPlColdStoreStorage::isNeedControlFreeSpace() 
{
    return true;
}

bool QnPlColdStoreStorage::removeFile(const QString& url) 
{
    return false;
}

bool QnPlColdStoreStorage::removeDir(const QString& url) 
{
    return false;
}

bool QnPlColdStoreStorage::isFileExists(const QString& url) 
{
    return false;
}

bool QnPlColdStoreStorage::isDirExists(const QString& url) 
{
    return false;
}

qint64 QnPlColdStoreStorage::getFreeSpace() 
{
    return 10*1024*1024*1024ll;
}
