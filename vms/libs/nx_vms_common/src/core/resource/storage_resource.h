// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_storage_resource.h"
#include <utils/crypt/encryptable.h>
#include <nx/vms/api/data/storage_status_list.h>

class QnAbstractMediaStreamDataProvider;

class NX_VMS_COMMON_API QnStorageResource:
    public QnAbstractStorageResource,
    public nx::utils::Encryptable
{
    using base_type = QnAbstractStorageResource;

    Q_OBJECT

    Q_PROPERTY(qint64 spaceLimit READ getSpaceLimit WRITE setSpaceLimit)
    Q_PROPERTY(int maxStoreTime READ getMaxStoreTime WRITE setMaxStoreTime)
public:
    static const qint64 kNasStorageLimit;
    static const qint64 kThirdPartyStorageLimit;

    virtual ~QnStorageResource();

    QnMediaServerResourcePtr getParentServer() const;

    void setSpaceLimit(qint64 value);
    virtual qint64 getSpaceLimit() const;

    void setStorageType(const QString& type);
    QString getStorageType() const;

    void setMaxStoreTime(int timeInSeconds);
    int getMaxStoreTime() const;

    void setUsedForWriting(bool isUsedForWriting);
    bool isUsedForWriting() const;

    void setStatusFlag(nx::vms::api::StorageStatuses status);
    nx::vms::api::StorageStatuses statusFlag() const;

    static QString urlToPath(const QString &url);
    static QString urlWithoutCredentials(const QString& url);
    QString urlWithoutCredentials() const;

    void fillID();
    static QnUuid fillID(const QnUuid& mserverId, const QString& url);
    bool isExternal() const;
    virtual bool isSystem() const { return false; }

    virtual qint64 bitrateBps() const;

    void addBitrate(QnAbstractMediaStreamDataProvider* provider);
    void releaseBitrate(QnAbstractMediaStreamDataProvider* provider);

    /*
     * Short and uniq storage ID. It is addition related ID field, and used for memory usage optimization
     */
    virtual void setUrl(const QString& value) override;

    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

    static QString toNativeDirPath(const QString &dirPath);

    void setBackup(bool value);
    bool isBackup() const;

    bool isWritable() const;
    bool isDbReady() const;

    virtual qint64 nxOccupedSpace() const { return 0; }

    virtual QIODevice* open(
        const QString &fileName,
        QIODevice::OpenMode openMode) override;

    virtual bool canStoreAnalytics() const;

signals:
    /*
     * Storage may emit archiveRangeChanged signal to inform server what some data in archive already deleted
     * @param newStartTime - new archive start time point
     * @param newEndTime - Not used now, reserved for future use
     */
    void archiveRangeChanged(const QnStorageResourcePtr& resource, qint64 newStartTimeMs,
        qint64 newEndTimeMs);

    void isUsedForWritingChanged(const QnResourcePtr& resource);
    void isBackupChanged(const QnResourcePtr& resource);
    void spaceLimitChanged(const QnResourcePtr& resource);
    void typeChanged(const QnResourcePtr& resource);
protected:
    virtual QIODevice* openInternal(const QString &fileName, QIODevice::OpenMode openMode) = 0;
private:
    qint64 m_spaceLimit = QnAbstractStorageResource::kUnknownSize;
    int m_maxStoreTime = 0; // in seconds
    bool m_usedForWriting = false;
    std::atomic<float> m_storageBitrateCoeff;
    QString m_storageType;
    QSet<QnAbstractMediaStreamDataProvider*> m_providers;
    mutable nx::Mutex m_bitrateMtx;
    bool m_isBackup = false;
    nx::vms::api::StorageStatuses m_status = nx::vms::api::StorageStatus::none;
};
