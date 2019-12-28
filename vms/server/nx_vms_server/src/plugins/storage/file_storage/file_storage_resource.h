#pragma once

#include "core/resource/storage_resource.h"
#include <platform/platform_abstraction.h>
#include <nx/vms/server/resource/storage_resource.h>
#include <nx/utils/lockable.h>

#if defined(Q_OS_WIN)
#include <wtypes.h>
#endif

#include <optional>
#include <atomic>

#include <optional>
#include <atomic>

/*
* QnFileStorageResource uses custom implemented IO access
*/

class QnStorageManager;
class QnMediaServerModule;

namespace nx { namespace vms::server { class RootFileSystem; } }

class QnFileStorageResource: public nx::vms::server::StorageResource
{
    using base_type = nx::vms::server::StorageResource;
private:
    static const QString FROM_SEP;
    static const QString TO_SEP;

public:
    QnFileStorageResource(QnMediaServerModule* serverModule);
    ~QnFileStorageResource();

    static QString tempFolderName();

    static QnStorageResource* instance(QnMediaServerModule* serverModule, const QString&);

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

    // true if storage is located on local disks
    bool isLocal();
    bool isMounted() const;

    // calculate space limit judging by partition type
    static qint64 calcSpaceLimit(
        QnMediaServerModule* serverModule,
        nx::vms::server::PlatformMonitor::PartitionType ptype);

    qint64 calcInitialSpaceLimit();

private:
    qint64 getDeviceSizeByLocalPossiblyNonExistingPath(const QString &path) const;
    QString removeProtocolPrefix(const QString& url);
    Qn::StorageInitResult initOrUpdateInternal();
    Qn::StorageInitResult updatePermissions(const QString& url) const;
    bool checkWriteCap() const;
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
    QIODevice* openInternal(const QString& fileName, QIODevice::OpenMode openMode);
    QIODevice* openInternal(const QString& url, QIODevice::OpenMode openMode, int bufferSize);

    void setIsSystemFlagIfNeeded();
    Qn::StorageInitResult initStorageDirectory(const QString& url);
    Qn::StorageInitResult initRemoteStorage(const QString& url);
    Qn::StorageInitResult checkMountedStatus() const;
    Qn::StorageInitResult testWrite() const;
    bool isValid() const;
    bool isLocalPathMounted(const QString& path) const;

public:
    // Try to remove old temporary dirs if any.
    // This could happen if server crashed and ~FileStorageResource
    // was not called.
    static void removeOldDirs(QnMediaServerModule *serverModule);

protected:
    mutable std::atomic<int64_t> m_cachedTotalSpace{ -1 };
    mutable std::atomic<Qn::StorageInitResult> m_state = Qn::StorageInit_CreateFailed;

    nx::vms::server::RootFileSystem* rootTool() const;
    QString getFsPath() const;

private:
    mutable boost::optional<bool> m_dbReady;
    mutable int m_capabilities = 0;
    QString m_localPath;
    nx::utils::Lockable<std::optional<bool>> m_isSystem;
    QnMediaServerModule* m_serverModule = nullptr;
};
typedef QSharedPointer<QnFileStorageResource> QnFileStorageResourcePtr;
