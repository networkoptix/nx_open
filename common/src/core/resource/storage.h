#ifndef QN_STORAGE_H
#define QN_STORAGE_H

#include "resource.h"

class QnStorage : public QnResource
{
    Q_OBJECT
    Q_PROPERTY(qint64 spaceLimit READ getSpaceLimit WRITE setSpaceLimit)
    Q_PROPERTY(int maxStoreTime READ getMaxStoreTime WRITE setMaxStoreTime)

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
    virtual void setUrl(const QString& value);
private:
    qint64 m_spaceLimit;
    int m_maxStoreTime; // at seconds
    quint16 m_index;
};

typedef QSharedPointer<QnStorage> QnStoragePtr;

#endif // QN_STORAGE_H
