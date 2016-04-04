#ifndef _FILE_STORAGE_PROTOCOL_H__
#define _FILE_STORAGE_PROTOCOL_H__

#include <atomic>
#include <libavformat/avio.h>
#include "core/resource/storage_resource.h"
#include <utils/common/app_info.h>
#include <platform/platform_abstraction.h>

/*
* QnFileStorageResource uses custom implemented IO access
*/

class QnStorageManager;

const QString NX_TEMP_FOLDER_NAME = QnAppInfo::productNameShort() + "_temp_folder_";

class QnFileStorageResource: public QnStorageResource
{
private:
    static const QString FROM_SEP;
    static const QString TO_SEP;
public:
    QnFileStorageResource();
    ~QnFileStorageResource();

    static QnStorageResource* instance(const QString&);

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual float getAvarageWritingUsage() const override;

    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& dirName) override;
    virtual qint64 getFileSize(const QString& url) const override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace() override;

    virtual int getCapabilities() const override;
    virtual bool initOrUpdate() const override;

    virtual void setUrl(const QString& url) override;

    QString getLocalPath() const
    {
        return m_localPath.isEmpty() ? getPath() : m_localPath;
    }

    // true if storage is located on local disks
    static bool isLocal(const QString &url);
    // calculate space limit judging by storage URL
    static qint64 calcSpaceLimit(const QString &url);
    // calculate space limit judging by partition type
    static qint64 calcSpaceLimit(QnPlatformMonitor::PartitionType ptype);

private:
    virtual QString getPath() const override;
    QString removeProtocolPrefix(const QString& url);
    bool initOrUpdateInternal() const;
    bool updatePermissions() const;
    bool checkWriteCap() const;
    bool isStorageDirMounted() const;
    bool checkDBCap() const;

    // It is for smb mount points with linux server .
    // Translates remote url to local temporary mount folder.
    // Should have no effect on another storage types and OS's.
    QString translateUrlToLocal(const QString &url) const;
    QString translateUrlToRemote(const QString &url) const;

    // mounts network (smb) folder to temporary local path
    // returns not 0 if something went wrong, 0 otherwise
    int mountTmpDrive() const;
    bool testWriteCapInternal() const;

    void setLocalPathSafe(const QString &path) const;
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
    mutable QString     m_localPath;

    mutable qint64 m_cachedTotalSpace;
    mutable boost::optional<bool> m_writeCapCached;
    mutable QnMutex      m_writeTestMutex;
};
typedef QSharedPointer<QnFileStorageResource> QnFileStorageResourcePtr;

#endif // _FILE_STORAGE_PROTOCOL_H__
