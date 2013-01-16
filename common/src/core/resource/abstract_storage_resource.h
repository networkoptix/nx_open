#ifndef __ABSTRACT_STORAGE_RESOURCE_H__
#define __ABSTRACT_STORAGE_RESOURCE_H__

#include "resource.h"

class QnAbstractMediaStreamDataProvider;

class QnAbstractStorageResource : public QnResource
{
    Q_OBJECT

    Q_PROPERTY(qint64 spaceLimit READ getSpaceLimit WRITE setSpaceLimit)
    Q_PROPERTY(int maxStoreTime READ getMaxStoreTime WRITE setMaxStoreTime)

public:
    QnAbstractStorageResource();
    virtual ~QnAbstractStorageResource();

    virtual QString getUniqueId() const;

    void setSpaceLimit(qint64 value);
    qint64 getSpaceLimit() const;

    void setMaxStoreTime(int timeInSeconds);
    int getMaxStoreTime() const;

    virtual float bitrate() const;
    virtual float getStorageBitrateCoeff() const { return 1.0; }

    void addBitrate(QnAbstractMediaStreamDataProvider* provider);
    void releaseBitrate(QnAbstractMediaStreamDataProvider* provider);

    /*
     * Short and uniq storage ID. It is addition related ID field, and used for memory usage optimization
     */
    void setIndex(quint16 value);
    quint16 getIndex() const;

    void deserialize(const QnResourceParameters& parameters);

    /*
     * Returns storage usage in range [0..1]
     */
    virtual float getAvarageWritingUsage() const;

signals:
    /*
     * Storage may emit archiveRangeChanged signal to inform server what some data in archive already deleted
     * @param newStartTime - new archive start time point
     * @param newEndTime - Not used now, reserved for future use
     */
    void archiveRangeChanged(const QnAbstractStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs);

private:
    qint64 m_spaceLimit;
    int m_maxStoreTime; // at seconds
    quint16 m_index;
    QSet<QnAbstractMediaStreamDataProvider*> m_providers;
    mutable QMutex m_bitrateMtx;
};

Q_DECLARE_METATYPE(QnAbstractStorageResourcePtr);
Q_DECLARE_METATYPE(QnAbstractStorageResourceList);

#endif // __ABSTRACT_STORAGE_RESOURCE_H__
