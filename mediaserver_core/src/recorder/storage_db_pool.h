#pragma once

#include "utils/db/db_helper.h"
#include "storage_db.h"
#include <QSharedPointer>

template <class  T>
class DependedSingleTone
{
private:
    static QWeakPointer<T> m_instance;
protected:
    DependedSingleTone<T>() {}
public:

    static T* instance() {
        return m_instance.data();
    }
    QSharedPointer<T> create()
    {
        QSharedPointer<T> result;
        if (m_instance)
            result = m_instance.toStrongRef();
        else {
            result = QSharedPointer<T>(new T());
            m_instance = result.toWeakRef();
        }
        return result;
    }
};

template<typename T> QWeakPointer<T> DependedSingleTone<T>::m_instance;

class QnStorageDbPool: public DependedSingleTone<QnStorageDbPool>
{
public:
    QnStorageDbPool();
    
    //static QnStorageDbPool* instance();

    QnStorageDbPtr getSDB(const QnStorageResourcePtr &storage);
    int getStorageIndex(const QnStorageResourcePtr& storage);
    void removeSDB(const QnStorageResourcePtr &storage);

    static bool getDBPath(const QnStorageResourcePtr& storage, QString* const dbDirectory);
    static QString getLocalGuid();
    void flush();
private:
    mutable QnMutex m_sdbMutex;
    mutable QnMutex m_mutexStorageIndex;
    
    QMap<QString, QnStorageDbPtr> m_chunksDB;
    QMap<QString, QSet<int> > m_storageIndexes;
};

#define qnStorageDbPool QnStorageDbPool::instance()
