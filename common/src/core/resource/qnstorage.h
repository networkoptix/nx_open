#ifndef __QNSTORAGE_H__
#define __QNSTORAGE_H__

#include "resource.h"

class QnStorage: public QnResource
{
public:
    QnStorage();
    virtual ~QnStorage();

    virtual QString getUniqueId() const;


    void setSpaceLimit(qint64 value);
    qint64 getSpaceLimit() const;

    void setMaxStoreTime(int timeInSeconds);
    int getMaxStoreTime() const;

    /*
     * Short and uniq storage ID. It is addition related ID field, and used for memory usage optimization
     */
    void setIndex(quint16 value);
    quint16 getIndex() const;

protected:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResource::ConnectionRole role);
private:
    qint64 m_spaceLimit;
    int m_maxStoreTime; // at seconds
    quint16 m_index;
};

typedef QSharedPointer<QnStorage> QnStoragePtr;

#endif // __QNSTORAGE_H__
