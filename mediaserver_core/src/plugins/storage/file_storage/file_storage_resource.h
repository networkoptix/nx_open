#ifndef _FILE_STORAGE_PROTOCOL_H__
#define _FILE_STORAGE_PROTOCOL_H__

#include <atomic>
#include "core/resource/storage_resource.h"
#include <utils/common/app_info.h>
#include <platform/platform_abstraction.h>

#if defined(Q_OS_WIN)
#include <wtypes.h>
#endif

/*
* QnFileStorageResource uses custom implemented IO access
*/

class QnStorageManager;

const QString NX_TEMP_FOLDER_NAME = QnAppInfo::productNameShort() + "_temp_folder_";

class QnFileStorageResource: public QnStorageResource
{
    using base_type = QnStorageResource;
private:
    static const QString FROM_SEP;
    static const QString TO_SEP;

public:
    QnFileStorageResource(QnCommonModule* commonModule);
    ~QnFileStorageResource();

    static QnStorageResource* instance(QnCommonModule* commonModule, const QString&);

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;
    QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode, int bufferSize);

    virtual float getAvarageWritingUsage() const override;

    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& dirName) override;
    virtual qint64 getFileSize(const QString& url) const override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace() const override;

    virtual int getCapabilities() const override;
    virtual Qn::StorageInitResult initOrUpdate() override;

    virtual void setUrl(const QString& url) override;
    virtual bool isSystem() const override;

    virtual QString getPath() const override;

    qint64 calculateAndSetTotalSpaceWithoutInit();

    // true if storage is located on local disks
    bool isLocal();
    // calculate space limit judging by partition type
    static qint64 calcSpaceLimit(QnPlatformMonitor::PartitionType ptype);

    qint64 calcInitialSpaceLimit();
private:

    QString removeProtocolPrefix(const QString& url);
    Qn::StorageInitResult initOrUpdateInternal();
    Qn::StorageInitResult updatePermissions(const QString& url) const;
    bool checkWriteCap() const;
    bool isStorageDirMounted() const;
    bool checkDBCap() const;
#if defined(Q_OS_WIN)
    Qn::StorageInitResult updatePermissionsHelper(
        LPWSTR userName,
        LPWSTR password,
        NETRESOURCE* netRes) const;
#endif

    // It is for smb mount points with linux server .
    // Translates remote url to local temporary mount folder.
    // Should have no effect on another storage types and OS's.
    QString translateUrlToLocal(const QString &url) const;
    QString translateUrlToRemote(const QString &url) const;

    // mounts network (smb) folder to temporary local path
    Qn::StorageInitResult mountTmpDrive(const QString& url);
    bool testWriteCapInternal() const;

    void setLocalPathSafe(const QString &path);
    QString getLocalPathSafe() const;
public:
    // Try to remove old temporary dirs if any.
    // This could happen if server crashed and ~FileStorageResource
    // was not called.
    static void removeOldDirs();

private:
    mutable std::atomic<bool> m_valid;
    mutable boost::optional<bool> m_dbReady;

private:
    mutable QnMutex     m_mutexCheckStorage;
    mutable int         m_capabilities;
    QString     m_localPath;

    mutable qint64 m_cachedTotalSpace;
    mutable boost::optional<bool> m_writeCapCached;
    mutable QnMutex      m_writeTestMutex;
    bool m_isSystem;
};
typedef QSharedPointer<QnFileStorageResource> QnFileStorageResourcePtr;

#endif // _FILE_STORAGE_PROTOCOL_H__
