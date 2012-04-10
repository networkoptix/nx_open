#ifndef QN_STORAGE_RESOURCE_H
#define QN_STORAGE_RESOURCE_H

#include <QSet>
#include "resource.h"


class QnAbstractMediaStreamDataProvider;

class QnStorageResource : public QnResource
{
    Q_OBJECT
    Q_PROPERTY(qint64 spaceLimit READ getSpaceLimit WRITE setSpaceLimit)
    Q_PROPERTY(int maxStoreTime READ getMaxStoreTime WRITE setMaxStoreTime)

public:
    QnStorageResource();
    virtual ~QnStorageResource();

    virtual QString getUniqueId() const;


    void setSpaceLimit(qint64 value);
    qint64 getSpaceLimit() const;

    void setMaxStoreTime(int timeInSeconds);
    int getMaxStoreTime() const;

    float bitrate() const;
    void addBitrate(QnAbstractMediaStreamDataProvider* provider);
    void releaseBitrate(QnAbstractMediaStreamDataProvider* provider);

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
    QSet<QnAbstractMediaStreamDataProvider*> m_providers;
};

#endif // QN_STORAGE_RESOURCE_H
