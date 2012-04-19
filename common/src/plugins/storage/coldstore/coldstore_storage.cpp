#include "coldstore_storage.h"

QnPlColdStoreStorage::QnPlColdStoreStorage()
{

}

QnStorageResource* QnPlColdStoreStorage::instance()
{
    return new QnPlColdStoreStorage();
}

QIODevice* QnPlColdStoreStorage::open(const QString& fileName, QIODevice::OpenMode openMode)
{
    return 0;
}

int QnPlColdStoreStorage::getChunkLen() const 
{
    return 15*60;
}

bool QnPlColdStoreStorage::isStorageAvailable() 
{
    Veracity::SFSClient csConnection;
    QByteArray ipba = coldstoreAddr().toLocal8Bit();
    char* ip = ipba.data();

    if (csConnection.Connect(ip) != Veracity::ISFS::STATUS_SUCCESS)
    {
        return false;
    }
    
    csConnection.Disconnect();
    
    return true;
}

QFileInfoList QnPlColdStoreStorage::getFileList(const QString& dirName) 
{
    QnStorageURL sUrl = url2StorageURL(dirName);
    QString csUrl = csURL(sUrl);

    return QFileInfoList();
}

bool QnPlColdStoreStorage::isNeedControlFreeSpace() 
{
    return false;
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


//=======private==============================

QString QnPlColdStoreStorage::coldstoreAddr() const
{
    QString url = getUrl();
    int prefix = url.indexOf("://");

    return prefix == -1 ? "" : url.mid(prefix + 3);
}

QString QnPlColdStoreStorage::csURL(const QnStorageURL& url) const
{
    QString serverID = getParentId().toString();

    QString result;

    QTextStream s(&result);
    
    s << serverID;

    if (url.resourceId=="")
        return result;
    s << "/" << url.resourceId;

    if (url.y=="")
        return result;
    s << "/" << url.y;

    if (url.m=="")
        return result;
    s << "/" << url.m;

    if (url.d=="")
        return result;
    s << "/" << url.d;

    if (url.h=="")
        return result;
    s << "/" << url.h;

    Q_ASSERT(result.length() < 63); // cold store filename limitation

    return result;

}