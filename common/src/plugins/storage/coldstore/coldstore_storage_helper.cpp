#include "coldstore_storage_helper.h"
#include "coldstore_connection_pool.h"
#include "coldstore_storage.h"

QnColdStoreMetaData::QnColdStoreMetaData():
m_mutex(QMutex::Recursive)
{
    m_needsToBesaved = false;
    m_lastUsageTime.restart();
}

QnColdStoreMetaData::~QnColdStoreMetaData()
{
    
}

void QnColdStoreMetaData::put(const QString& fn, QnCSFileInfo info)
{
    QMutexLocker lock(&m_mutex);

    m_lastUsageTime.restart();
    m_hash.insert(fn, info);
}

bool QnColdStoreMetaData::hasSuchFile(const QString& fn) const
{
    QMutexLocker lock(&m_mutex);

    m_lastUsageTime.restart();
    return m_hash.contains(fn);
}

QnCSFileInfo QnColdStoreMetaData::getFileinfo(const QString& fn) const
{
    QMutexLocker lock(&m_mutex);

    m_lastUsageTime.restart();
    return m_hash.value(fn);
}

QFileInfoList QnColdStoreMetaData::fileInfoList(const QString& subPath) const
{
    QMutexLocker lock(&m_mutex);

    QFileInfoList result;

    QHashIterator<QString, QnCSFileInfo> it(m_hash);
    while (it.hasNext()) 
    {
        it.next();
        QString fn = it.key();
        
        if (fn.contains(subPath))
            result.push_back(QFileInfo(fn));
    }

    return result;

}

QByteArray QnColdStoreMetaData::toByteArray() const
{
    QMutexLocker lock(&m_mutex);

    QByteArray result;
    result.reserve(m_hash.size()*66 + 4); // to avoid memory reallocation

    result.append("1;"); //version

    QHashIterator<QString, QnCSFileInfo> it(m_hash);
    while (it.hasNext()) 
    {
        it.next();
        QString fn = it.key();
        QnCSFileInfo fi = it.value();

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
    QMutexLocker lock(&m_mutex);

    m_hash.clear();

    QList<QByteArray> lst = ba.split(';');
    if (lst.size()==0)
        return;

    QString version = QString(lst.at(0));

    for (int i = 1; i < lst.size(); i+=3 )
    {
        QString fn = QString ( lst.at(i+0) );
        quint64 shift = QString ( lst.at(i+1) ).toLongLong(0, 16);
        quint64 len = QString ( lst.at(i+2) ).toLongLong(0, 16);

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
m_mutex(QMutex::Recursive)
{

}

QnColdStoreMetaDataPool::~QnColdStoreMetaDataPool()
{
    QMutexLocker lock(&m_mutex);
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

        if (connection.open(csFn, QIODevice::ReadOnly, CS_META_DATA_CHANNEL))
        {
            md = QnColdStoreMetaDataPtr(new QnColdStoreMetaData());

            quint64 size = connection.size();
            QByteArray ba;
            ba.resize(size);
            connection.read(ba.data(), size);
            md->fromByteArray(ba);

            QMutexLocker lock(&m_mutex);
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
    QMutexLocker lock(&m_mutex);

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
