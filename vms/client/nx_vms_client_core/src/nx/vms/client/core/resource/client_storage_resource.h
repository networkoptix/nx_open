// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/storage_resource.h>
#include <nx/vms/api/data/storage_init_result.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

/*
*   This class used only for manipulating storage as User Interface entity.
*   Additional info like total space is loaded with a separate API request
*   and is set from outside.
*/
class NX_VMS_CLIENT_CORE_API QnClientStorageResource: public QnStorageResource
{
    Q_OBJECT

    typedef QnStorageResource base_type;
public:
    enum StorageFlags {
        ReadOnly        = 0x1,
        ContainsCameras = 0x2,
    };

    QnClientStorageResource();
    virtual ~QnClientStorageResource();

    static QnClientStorageResourcePtr newStorage(const QnMediaServerResourcePtr &parentServer, const QString &url);

    virtual nx::vms::api::StorageInitResult initOrUpdate() override;
    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& dirName) override;
    qint64 getFileSize(const QString& url) const override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;

    void setFreeSpace(qint64 value);
    virtual qint64 getFreeSpace() override;

    void setTotalSpace(qint64 value);
    virtual qint64 getTotalSpace() const override;

    void setWritable(bool isWritable);
    virtual int getCapabilities() const override;

    bool isActive() const;
    void setActive(bool value);

protected:
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;
    virtual QIODevice* openInternal(const QString& fileName, QIODevice::OpenMode openMode) override;
signals:
    void freeSpaceChanged(const QnResourcePtr &storage);
    void totalSpaceChanged(const QnResourcePtr &storage);
    void isWritableChanged(const QnResourcePtr &storage);
    void isActiveChanged(const QnResourcePtr &storage);

protected:
    /** Check if space info is loaded for the given resource. */
    bool isSpaceInfoAvailable() const;

private:
    qint64 m_freeSpace = QnAbstractStorageResource::kUnknownSize;
    qint64 m_totalSpace = QnAbstractStorageResource::kUnknownSize;
    bool m_writable = false;

    /**
     *  Flag: storage really exists in server-side resource pool.
     *  Storages that won't have this flag:
     *      - newly added remote storages, until changes are applied
     *      - auto-found server-side partitions without storage
     */
    bool m_active = false;
};
