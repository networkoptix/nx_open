#include "coldstore_storage.h"

QnPlColdStoreStorage::QnPlColdStoreStorage()
{

}

URLProtocol QnPlColdStoreStorage::getURLProtocol() const
{
    return URLProtocol();
}

int QnPlColdStoreStorage::getChunkLen() const 
{
    return 60*60;
}

bool QnPlColdStoreStorage::isStorageAvailable(const QString& value) 
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

qint64 QnPlColdStoreStorage::getFreeSpace(const QString& url) 
{
    return 10*1024*1024*1024;
}
