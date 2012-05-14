#include "coldstore_storage.h"
#include "coldstore_io_buffer.h"
#include "utils/common/sleep.h"

QnPlColdStoreStorage::QnPlColdStoreStorage():
m_connectionPool(this),
m_metaDataPool(this),
m_mutex(QMutex::Recursive),
m_cswriterThread(0),
m_currentConnection(0),
m_prevConnection(0)
{
    
}

QnPlColdStoreStorage::~QnPlColdStoreStorage()
{
    if (m_cswriterThread)
        m_cswriterThread->stop();

    delete m_cswriterThread;

    delete m_currentConnection;
    delete m_prevConnection;

}

QnStorageResource* QnPlColdStoreStorage::instance()
{
    return new QnPlColdStoreStorage();
}

QIODevice* QnPlColdStoreStorage::open(const QString& fileName, QIODevice::OpenMode openMode)
{

    if (openMode == QIODevice::ReadOnly)
    {
        QnCSFileInfo fi = getFileInfo(fileName);
        if (fi.len == 0)
        {
            return 0;
        }
        else
        {
            QString csFileName = fileName2csFileName(fileName);

            QnColdStoreIOBuffer* buff = new QnColdStoreIOBuffer(toSharedPointer(), fileName);
            buff->open(QIODevice::WriteOnly);
            buff->buffer().resize(fi.len);
            m_connectionPool.read(fileName, buff->buffer().data(), fi.shift, fi.len);
            buff->open(QIODevice::ReadOnly);

        }

    }
    else if (openMode == QIODevice::WriteOnly)
    {
        QnColdStoreIOBuffer* buff = new QnColdStoreIOBuffer(toSharedPointer(), fileName);
        buff->open(QIODevice::WriteOnly);
    
        {
            QMutexLocker lock(&m_mutex);
            m_listOfWritingFiles.insert(fileName);
        }
        

        return buff;
    }

    return 0;

}

void QnPlColdStoreStorage::onWriteBuffClosed(QnColdStoreIOBuffer* buff)
{
    QMutexLocker lock(&m_mutex);

    if (!m_cswriterThread)
    {
        m_cswriterThread = new QnColdStoreWriter(toSharedPointer());
        m_cswriterThread->start();
    }

    // here I assume if you copy QByteArray you do not copy memory 
    while (m_cswriterThread->queueSize() > 20)
        QnSleep::msleep(2); // to avoid crash( out of memory ) if write speed is less than in data

    m_cswriterThread->write(buff->buffer(), buff->getFileName());
}

void QnPlColdStoreStorage::onWrite(const QByteArray& ba, const QString& fn)
{
    QString csFileName = fileName2csFileName(fn);

    QMutexLocker lock(&m_mutex);

    QnColdStoreConnection* connection = getPropriteConnectionForCsFile(csFileName);
    if (connection==0)
        return;

    //QnColdStoreMetaDataPtr md = connection == 

}

int QnPlColdStoreStorage::getChunkLen() const 
{
    return 10*60; // 10 sec
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

bool QnPlColdStoreStorage::isStorageAvailableForWriting()
{
    return isStorageAvailable();
}


QFileInfoList QnPlColdStoreStorage::getFileList(const QString& dirName) 
{
    QString csFileName = fileName2csFileName(dirName);

    
    QnColdStoreMetaDataPtr md = getMetaDataFileForCsFile(csFileName);

    QMutexLocker lock(&m_mutex);
    // also add files still open files 
    QFileInfoList openlst;
    foreach(QString fn, m_listOfWritingFiles)
        openlst.push_back(QFileInfo(fn));

    if (md)
    {
        
        QFileInfoList result = md->fileInfoList(dirName);
        result << openlst;
        return result;
    }
    else
    {
        return openlst;
    }

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

QnCSFileInfo QnPlColdStoreStorage::getFileInfo(const QString& fn)
{
    QString csFileName = fileName2csFileName(fn);

    QnColdStoreMetaDataPtr md = getMetaDataFileForCsFile(csFileName);

    if (!md)
        return QnCSFileInfo();

    return md->getFileinfo(fn);
}

QString QnPlColdStoreStorage::fileName2csFileName(const QString& fn) const
{
    return csDataFileName(url2StorageURL(fn));
}
//=======private==============================
bool QnPlColdStoreStorage::hasOpenFilesFor(const QString& csFile) const
{
    QMutexLocker lock(&m_mutex);
    foreach(QString fn, m_listOfWritingFiles)
    {
        if (fileName2csFileName(fn) == csFile)
            return true;
    }

    return false;
}

QnColdStoreConnection* QnPlColdStoreStorage::getPropriteConnectionForCsFile(const QString& csFile)
{
    QMutexLocker lock(&m_mutex);

    if (m_currentConnection == 0)
    {
        m_currentConnection = new QnColdStoreConnection(coldstoreAddr());
        if (!m_currentConnection->open(csFile, QIODevice::WriteOnly, 0))
        {
        
            delete m_currentConnection;
            m_currentConnection = 0;
            return 0;
        }
        return m_currentConnection;
    }

    if (m_prevConnection==0 && m_currentConnection->getFilename() == csFile)
        return m_currentConnection;

    if (m_prevConnection && m_prevConnection->getFilename() == csFile)
        return m_prevConnection;

    Q_ASSERT(false);
    return 0;

}


QnColdStoreMetaDataPtr QnPlColdStoreStorage::getMetaDataFileForCsFile(const QString& csFile)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_currentWritingFileMetaData && m_currentWritingFileMetaData->csFileName() == csFile)
        {
            return m_currentWritingFileMetaData;
        }
        else if (m_prevWritingFileMetaData && m_prevWritingFileMetaData->csFileName() == csFile)
        {
            return m_prevWritingFileMetaData;
        }
    }

    return m_metaDataPool.getStoreMetaData(csFile);
}


QString QnPlColdStoreStorage::coldstoreAddr() const
{
    QString url = getUrl();
    int prefix = url.indexOf("://");

    return prefix == -1 ? "" : url.mid(prefix + 3);
}


QString QnPlColdStoreStorage::csDataFileName(const QnStorageURL& url) const
{
    QString serverID = getParentId().toString();

    QString result;

    QTextStream s(&result);
    
    s << serverID;

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