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

    void setUsedForWriting(bool isUsedForWriting);
    bool isUsedForWriting() const;
    QString getPath() const;
    static QString urlToPath(const QString& url);

    static QnUuid fillID(const QnUuid& mserverId, const QString& url);
    bool isExternal() const;
#ifdef ENABLE_DATA_PROVIDERS
    virtual float bitrate() const;
    virtual float getStorageBitrateCoeff() const { return 1.0; }

    void addBitrate(QnAbstractMediaStreamDataProvider* provider);
    void releaseBitrate(QnAbstractMediaStreamDataProvider* provider);
#endif

    /*
     * Short and uniq storage ID. It is addition related ID field, and used for memory usage optimization
     */
    virtual void setUrl(const QString& value) override;

    /*
     * Returns storage usage in range [0..1]
     */
    virtual float getAvarageWritingUsage() const;

    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;
signals:
    /*
     * Storage may emit archiveRangeChanged signal to inform server what some data in archive already deleted
     * @param newStartTime - new archive start time point
     * @param newEndTime - Not used now, reserved for future use
     */
    void archiveRangeChanged(const QnAbstractStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs);
private:
    qint64 m_spaceLimit;
    int m_maxStoreTime; // in seconds
    bool m_usedForWriting;
    QSet<QnAbstractMediaStreamDataProvider*> m_providers;
    mutable QnMutex m_bitrateMtx;
};

Q_DECLARE_METATYPE(QnAbstractStorageResourcePtr);
Q_DECLARE_METATYPE(QnAbstractStorageResourceList);

#endif // __ABSTRACT_STORAGE_RESOURCE_H__
