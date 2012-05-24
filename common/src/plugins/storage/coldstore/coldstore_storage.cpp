#include "coldstore_storage.h"
#include "coldstore_io_buffer.h"
#include "utils/common/sleep.h"



QnPlColdStoreStorage::QnPlColdStoreStorage():
m_connectionPool(this),
m_metaDataPool(this),
m_mutex(QMutex::Recursive),
m_cswriterThread(0),
m_currH(0),
m_prevH(0)
{
    
    /*
    char dataW[1024*1024/2]; dataW[7] = 7;
    //char dataR[1024]; dataR[7] = 8;

    QnColdStoreConnection connW("10.10.10.59");
    bool b = connW.open("333", QIODevice::WriteOnly, 0);

    QnSleep::sleep(60*2);

    for (int i = 0; i < 100*2; ++i)
        connW.write(dataW, sizeof(dataW));

    connW.close();

    
    
    QnColdStoreConnection connW1("10.10.10.59");
    b = connW1.open("222", QIODevice::WriteOnly, 1);
    connW1.write(dataW, sizeof(dataW));
    connW1.close();
    

    
    QnColdStoreConnection connR("10.10.10.59");
    b = connR.open("333", QIODevice::ReadOnly, 0);
    b = connR.seek(1024*1024*50);
    int r = connR.read(dataW, sizeof(dataW));
    connR.close();
    /**/
    
}

QnPlColdStoreStorage::~QnPlColdStoreStorage()
{
    if (m_cswriterThread)
        m_cswriterThread->stop();

    delete m_cswriterThread;

    delete m_currH;
    delete m_prevH;

}

QnStorageResource* QnPlColdStoreStorage::instance()
{
    return new QnPlColdStoreStorage();
}

QIODevice* QnPlColdStoreStorage::open(const QString& fileName, QIODevice::OpenMode openMode)
{
    //coldstore://10.10.10.59
    
    QString nfileName = normolizeFileName(fileName);

    if (openMode == QIODevice::ReadOnly)
    {
        QnCSFileInfo fi = getFileInfo(nfileName);
        if (fi.len == 0)
        {
            return 0;
        }
        else
        {
            QString csFileName = fileName2csFileName(nfileName);

            QnColdStoreIOBuffer* buff = new QnColdStoreIOBuffer(toSharedPointer(), nfileName);
            buff->open(QIODevice::ReadOnly);
            buff->buffer().resize(fi.len);
            if (m_connectionPool.read(csFileName, buff->buffer().data(), fi.shift, fi.len)>0)
            {
                //buff->setOpenMode(QIODevice::ReadOnly);

                qWarning() << "read file " << fileName <<"("<< csFileName << ") sh = " << fi.shift;

                return buff;
            }
            else
            {
                delete buff;
                return 0;
            }

        }

    }
    else if (openMode == QIODevice::WriteOnly)
    {
        QnColdStoreIOBuffer* buff = new QnColdStoreIOBuffer(toSharedPointer(), nfileName);
        buff->open(QIODevice::WriteOnly);
    
        {
            QMutexLocker lock(&m_mutex);
            m_listOfWritingFiles.insert(nfileName);
            //qWarning() << "file started : " << nfileName;

            //m_listOfExistingFiles.insert(nfileName);
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
    

    QnCsTimeunitConnectionHelper* connectionH = getPropriteConnectionForCsFile(csFileName);
    if (connectionH==0)
    {
        if (m_currH==0)
        {
            // must not be unless network issue 
            m_listOfWritingFiles.remove(fn);
            return;
        }

        // must be a new h; need to swap;
        Q_ASSERT(m_prevH == 0); // old h should not exist any mor 
        m_prevH = m_currH;
        
        // lets check if this is the last one 
        if (!hasOpenFilesFor(m_prevH->getCsFileName()))
        {
            m_prevH->close();
            delete m_prevH;
            m_prevH = 0;
        }


        m_currH = new QnCsTimeunitConnectionHelper();
        if (!m_currH->open(coldstoreAddr(), csFileName))
        {
            delete m_currH;
            m_currH = 0;
            m_listOfWritingFiles.remove(fn);
            return;
        }
        
        //write into new curr 
        m_mutex.unlock();
        m_currH->write(ba, fn);
        m_mutex.lock();

        

        
    }
    else if (connectionH == m_prevH)
    {
        // file from prev hour 
        // lets check if this is the last one 
        if (!hasOpenFilesFor(m_prevH->getCsFileName()))
        {
            // write into prev and close it

            m_mutex.unlock();
            m_prevH->write(ba, fn);
            m_mutex.lock();

            m_prevH->close();
            delete m_prevH;
            m_prevH = 0;

        }
        else
        {
            // keep saving to prev h
            m_mutex.unlock();
            m_prevH->write(ba, fn);
            m_mutex.lock();

        }

    }
    else if (connectionH == m_currH)
    {
        // keep saving to current h
        m_mutex.unlock();
        m_currH->write(ba, fn);
        m_mutex.lock();

    }

    m_listOfWritingFiles.remove(fn);

    //qWarning() << "file closed: " << fn;

    //QnColdStoreMetaDataPtr md = connectionH == 

}

int QnPlColdStoreStorage::getChunkLen() const 
{
    return 10; // 10 sec
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
    QString ndirName = normolizeFileName(dirName);

    QString csFileName = fileName2csFileName(dirName);

    

    
    QnColdStoreMetaDataPtr md = getMetaDataFileForCsFile(csFileName);

    QMutexLocker lock(&m_mutex);
    // also add files still open files 
    QFileInfoList result;
    foreach(QString fn, m_listOfWritingFiles)
        result.push_back(QFileInfo(fn));

    //foreach(QString fn, m_listOfExistingFiles)
    //    openlst.push_back(QFileInfo(fn));


    if (md)
    {
        result << md->fileInfoList(ndirName);;
    }

    
    foreach(QFileInfo fi, result)
    {
        qWarning() << fi.absoluteFilePath();
    }
    /**/

    return result;

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
    return true;
}

qint64 QnPlColdStoreStorage::getFreeSpace() 
{
    return 10*1024*1024*1024ll;
}

QnCSFileInfo QnPlColdStoreStorage::getFileInfo(const QString& fn)
{
    QString nfn = normolizeFileName(fn);
    QString csFileName = fileName2csFileName(nfn);

    QnColdStoreMetaDataPtr md = getMetaDataFileForCsFile(csFileName);

    if (!md)
        return QnCSFileInfo();

    return md->getFileinfo(nfn);
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

QnCsTimeunitConnectionHelper* QnPlColdStoreStorage::getPropriteConnectionForCsFile(const QString& csFile)
{
    QMutexLocker lock(&m_mutex);

    if (m_currH == 0)
    {
        m_currH = new QnCsTimeunitConnectionHelper();

        if (!m_currH->open(coldstoreAddr(),csFile))
        {
            delete m_currH;
            m_currH = 0;
            return 0;
        }

        return m_currH;
    }

    if (m_currH && m_currH->getCsFileName() == csFile)
        return m_currH;

    if (m_prevH && m_prevH->getCsFileName() == csFile)
        return m_prevH;

    //Q_ASSERT(false);
    return 0;

}


QnColdStoreMetaDataPtr QnPlColdStoreStorage::getMetaDataFileForCsFile(const QString& csFile)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_currH && m_currH->getCsFileName() == csFile)
        {
            return m_currH->getMD();
        }
        else if (m_prevH && m_prevH->getCsFileName() == csFile)
        {
            return m_prevH->getMD();
        }
    }

    return m_metaDataPool.getStoreMetaData(csFile);
}


QString QnPlColdStoreStorage::coldstoreAddr() const
{
    QString url = getUrl();
    int prefix = url.indexOf("://");

    return prefix == -1 ? url : url.mid(prefix + 3);
}

QString QnPlColdStoreStorage::normolizeFileName(const QString& fn) const
{
    QString addr = coldstoreAddr();

    QString result;

    int index = fn.indexOf(addr);
    if (index<0)
        result =  fn;
    else
    {
        if (fn.length() > index + addr.length())
            result =  fn.mid(index + addr.length() + 1);
        else
            result = fn.mid(index + addr.length());
    }

    if (result.endsWith("/"))
        result = result.left(result.length() - 1);

    return result;

}


QString QnPlColdStoreStorage::fileName2csFileName(const QString& fn) const
{
    QString nfn = normolizeFileName(fn);

    QString serverID = getParentId().toString();
    QString result;

    QTextStream s(&result);
    
    s << serverID;

    QStringList restList = nfn.split("/", QString::SkipEmptyParts);
    if (restList.size() ==0 )
        return result;

    if (restList.size() > 2)
    {
        s << "_";
        s << restList.at(2); // 
    }

    s << "_";

    if (restList.size() > 3)
        s << restList.at(3); // 

    s << "_";

    if (restList.size() > 4)
        s << restList.at(4); // 

    s << "_";

    if (restList.size() > 5)
        s << restList.at(5); // 

    //==============================================
    /*
    s << "_";

    if (restList.size() > 6)
        s << restList.at(6); // 
        /**/



    Q_ASSERT(result.length() < 63); // cold store filename limitation

    return result;

}

