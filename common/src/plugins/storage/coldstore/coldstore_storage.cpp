#include "coldstore_storage.h"

QnPlColdStoreStorage::QnPlColdStoreStorage()
{

}

QIODevice* QnPlColdStoreStorage::open(const QString& fileName, QIODevice::OpenMode openMode)
{
    return 0;
}

int QnPlColdStoreStorage::getChunkLen() const 
{
    return 60*60;
}

bool QnPlColdStoreStorage::isStorageAvailable() 
{
    // todo: implement me
    return true;
}

bool QnPlColdStoreStorage::isStorageAvailableForWriting()
{
    // todo: implement me
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

bool QnPlColdStoreStorage::renameFile(const QString& oldName, const QString& newName)
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
