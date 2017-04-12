#ifndef __STORAGE_RESOURCE_H__
#define __STORAGE_RESOURCE_H__

#include "abstract_storage_resource.h"
#include <atomic>

class QnAbstractMediaStreamDataProvider;

class QnStorageResource: public QnAbstractStorageResource
{
    using base_type = QnAbstractStorageResource;

    Q_OBJECT

    Q_PROPERTY(qint64 spaceLimit READ getSpaceLimit WRITE setSpaceLimit)
    Q_PROPERTY(int maxStoreTime READ getMaxStoreTime WRITE setMaxStoreTime)
public:
    static const qint64 kNasStorageLimit;
    static const qint64 kThirdPartyStorageLimit;

    QnStorageResource(QnCommonModule* commonModule);
    virtual ~QnStorageResource();

    virtual QString getUniqueId() const;

    QnMediaServerResourcePtr getParentServer() const;

    void setStorageBitrateCoeff(float value);
    void setSpaceLimit(qint64 value);
    virtual qint64 getSpaceLimit() const;

    void setStorageType(const QString& type);
    QString getStorageType() const;

    void setMaxStoreTime(int timeInSeconds);
    int getMaxStoreTime() const;

    void setUsedForWriting(bool isUsedForWriting);
    bool isUsedForWriting() const;

    virtual QString getPath() const;
    static QString urlToPath(const QString &url);
    static QString urlWithoutCredentials(const QString& url);

    static QnUuid fillID(const QnUuid& mserverId, const QString& url);
    bool isExternal() const;
    virtual bool isSystem() const { return false; }
#ifdef ENABLE_DATA_PROVIDERS
    virtual float bitrate() const;
    virtual float getStorageBitrateCoeff() const { return m_storageBitrateCoeff; }

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

    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers) override;

    static QString toNativeDirPath(const QString &dirPath);

    void setBackup(bool value);
    bool isBackup() const;

    bool isWritable() const;
signals:
    /*
     * Storage may emit archiveRangeChanged signal to inform server what some data in archive already deleted
     * @param newStartTime - new archive start time point
     * @param newEndTime - Not used now, reserved for future use
     */
    void archiveRangeChanged(const QnStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs);

    void isUsedForWritingChanged(const QnResourcePtr &resource);
    void isBackupChanged(const QnResourcePtr &resource);
private:
    qint64 m_spaceLimit;
    int m_maxStoreTime; // in seconds
    bool m_usedForWriting;
    std::atomic<float> m_storageBitrateCoeff;
    QString m_storageType;
    QSet<QnAbstractMediaStreamDataProvider*> m_providers;
    mutable QnMutex m_bitrateMtx;
    bool m_isBackup;
};

Q_DECLARE_METATYPE(QnStorageResourcePtr);
Q_DECLARE_METATYPE(QnStorageResourceList);

#endif // __ABSTRACT_STORAGE_RESOURCE_H__
