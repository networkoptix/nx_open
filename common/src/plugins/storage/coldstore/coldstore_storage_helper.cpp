#include "coldstore_storage_helper.h"

#ifdef ENABLE_COLDSTORE

#include "coldstore_connection_pool.h"
#include "coldstore_storage.h"

QnColdStoreMetaData::QnColdStoreMetaData():
m_mutex(QnMutex::Recursive)
{
    m_needsToBesaved = false;
    m_lastUsageTime.restart();
}

QnColdStoreMetaData::~QnColdStoreMetaData()
{
    
}

void QnColdStoreMetaData::put(const QString& fn, QnCSFileInfo info)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    m_lastUsageTime.restart();
    m_hash.insert(fn, info);
}


QnCSFileInfo QnColdStoreMetaData::getFileinfo(const QString& fn) const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    m_lastUsageTime.restart();
    QHash<QString, QnCSFileInfo>::const_iterator it = m_hash.find(fn);

    if (it == m_hash.end())
        return QnCSFileInfo();
    else
        return it.value();
}

QFileInfoList QnColdStoreMetaData::fileInfoList(const QString& subPath) const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QFileInfoList result;

    for(QHash<QString, QnCSFileInfo>::const_iterator pos = m_hash.begin(); pos != m_hash.end(); pos++) {
        QString fn = pos.key();
        
        if (fn.contains(subPath))
            result.push_back(QFileInfo(fn));
    }

    return result;

}

QByteArray QnColdStoreMetaData::toByteArray() const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QByteArray result;
    result.reserve(m_hash.size()*66 + 4); // to avoid memory reallocation

    result.append("1;"); //version

    for(QHash<QString, QnCSFileInfo>::const_iterator pos = m_hash.begin(); pos != m_hash.end(); pos++) {
        QString fn = pos.key();
        QnCSFileInfo fi = pos.value();

        result.append(fn);
        result.append(";");

        result.append(QString::number(fi.shift, 16));
        result.append(";");

        result.append(QString::number(fi.len, 16));
        result.append(";");

    }    

    return result;
}

void QnColdStoreMetaData::fromByteArray(const QByteArray& ba)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    m_hash.clear();

    QList<QByteArray> lst = ba.split(';');
    if (lst.size()==0)
        return;

    QString version = QLatin1String(lst.at(0));

    for (int i = 1; i < lst.size() - 3; i+=3 )
    {
        QString fn = QLatin1String( lst.at(i+0) );
        quint64 shift = QString(QLatin1String( lst.at(i+1) )).toLongLong(0, 16);
        quint64 len = QString(QLatin1String( lst.at(i+2) )).toLongLong(0, 16);

        m_hash.insert(fn, QnCSFileInfo(shift, len));
    }
}

bool QnColdStoreMetaData::needsToBesaved() const
{
    return m_needsToBesaved;
}

void QnColdStoreMetaData::setNeedsToBesaved(bool v)
{
    m_needsToBesaved = v;
}

int QnColdStoreMetaData::age() const
{
    return m_lastUsageTime.elapsed()/1000;
}


//====================================================

QnColdStoreMetaDataPool::QnColdStoreMetaDataPool(QnPlColdStoreStorage *csStorage):
m_csStorage(csStorage),
m_mutex(QnMutex::Recursive)
{

}

QnColdStoreMetaDataPool::~QnColdStoreMetaDataPool()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_pool.clear();
}

QnColdStoreMetaDataPtr QnColdStoreMetaDataPool::getStoreMetaData(const QString& csFn)
{

    checkIfSomedataNeedstoberemoved();

    m_mutex.lock();
    QHash<QString, QnColdStoreMetaDataPtr>::iterator it = m_pool.find(csFn);
    

    QnColdStoreMetaDataPtr md;

    if (it == m_pool.end())
    {
        m_mutex.unlock();

        QnColdStoreConnection connection(m_csStorage->coldstoreAddr());

        //if (connection.open(csFn, QIODevice::ReadOnly, CS_META_DATA_CHANNEL))

        if (connection.open(csFn + QLatin1String("_md"), QIODevice::ReadOnly, 0))
        {
            md = QnColdStoreMetaDataPtr (new QnColdStoreMetaData() );

            quint64 size = connection.size();
            QByteArray ba;
            ba.resize(size);
            connection.read(ba.data(), size);
            md->fromByteArray(ba);

            SCOPED_MUTEX_LOCK( lock, &m_mutex);
            m_pool.insert(csFn, md);
            return md;
        }
        else
        {
            return QnColdStoreMetaDataPtr(0);
            
        }
    }
    else
    {
        md = it.value();
        m_mutex.unlock();

        return md;
    }



}

void QnColdStoreMetaDataPool::checkIfSomedataNeedstoberemoved()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    if (m_pool.size() <= 20)
        return;

    QHash<QString, QnColdStoreMetaDataPtr>::iterator it = m_pool.begin();
    while (it != m_pool.end()) 
    {
        if (it.value()->age() > 40) // if last request to this connection was made more than 40 sec ago 
        {
            it = m_pool.erase(it);
        }
        else
            ++it;
    }

}

//=============================================================

QnCsTimeunitConnectionHelper::QnCsTimeunitConnectionHelper()
{
    m_connect = 0;
    m_metaData = QnColdStoreMetaDataPtr(new QnColdStoreMetaData());
}

QnCsTimeunitConnectionHelper::~QnCsTimeunitConnectionHelper()
{
    close();
}

bool QnCsTimeunitConnectionHelper::open(const QString& addr, const QString& csFile)
{
    m_addr = addr;
    m_csFileName = csFile;

    m_connect = new QnColdStoreConnection(m_addr);
    return m_connect->open(csFile, QIODevice::WriteOnly, CS_ACTUAL_DATA_CHANNEL);
}

void QnCsTimeunitConnectionHelper::write(const QByteArray& ba, const QString& fn)
{
    qint64 pos = m_connect->pos();
    m_connect->write(ba.data(), ba.size());

    QnCSFileInfo fi;
    fi.len = ba.size();
    fi.shift = pos;
    m_metaData->put(fn, fi);

}

void QnCsTimeunitConnectionHelper::close()
{
    if (!m_connect)
        return;
    
    QnColdStoreConnection conn(m_addr);

    if (conn.open(m_connect->getFilename() + QLatin1String("_md"), QIODevice::WriteOnly, 0))
    {
        QByteArray ba = m_metaData->toByteArray();
        conn.write(ba.data(), ba.size());
    }

    conn.close();

    qWarning() << "CS closing file " << m_connect->getFilename() + QLatin1String("_md") << " dataFile=" << m_csFileName;


    m_connect->close();
    delete m_connect;

    m_connect = 0;

}

QnColdStoreMetaDataPtr QnCsTimeunitConnectionHelper::getMD() 
{
    return m_metaData;
}

QString QnCsTimeunitConnectionHelper::getCsFileName() const
{
    return m_csFileName;
}

#endif // ENABLE_COLDSTORE
