#pragma once

#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <recording/time_period_list.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/vms/server/resource/storage_resource.h>
#include <server/server_globals.h>
#include <recorder/storage_manager.h>

class MediaServerLauncher;
class QnMediaServerModule;
class QnArchiveStreamReader;

namespace nx::vms::server::test::test_support {

/**
 * Adds a real storage which can be used for storing files, reindexing e.t.c.
 */
QnStorageResourcePtr addStorage(
    MediaServerLauncher* server, const QString& path, QnServer::StoragePool storageRole);

class StorageStub: public StorageResource
{
public:
    QString url;
    int64_t totalSpace = -1;
    int64_t freeSpace = -1;
    bool isSystemFlag = false;
    bool isOnlineFlag = false;

    StorageStub(
        QnMediaServerModule* serverModule, const QString& url, int64_t totalSpace,
        int64_t freeSpace, int64_t spaceLimit, bool isSystem, bool isOnline, bool isUsedForWriting)
        :
        StorageResource(serverModule), url(url), totalSpace(totalSpace),
        freeSpace(freeSpace), isSystemFlag(isSystem), isOnlineFlag(isOnline)
    {
        setSpaceLimit(spaceLimit);
        setUsedForWriting(isUsedForWriting);
        setIdUnsafe(QnUuid::createUuid());
    }

    virtual QIODevice* openInternal(const QString&, QIODevice::OpenMode ) override { return nullptr; }
    virtual FileInfoList getFileList(const QString&) override { return FileInfoList(); }
    virtual qint64 getFileSize(const QString&) const override { return -1LL; }
    virtual bool removeFile(const QString&) override { return true; }
    virtual bool removeDir(const QString&) override { return true; }
    virtual bool renameFile(const QString& , const QString& ) override { return true; }
    virtual bool isFileExists(const QString& ) override { return true; }
    virtual bool isDirExists(const QString& ) override { return true; }
    virtual qint64 getFreeSpace() override { return freeSpace; }
    virtual qint64 getTotalSpace() const override { return totalSpace; }
    virtual int getCapabilities() const override { return ListFile | RemoveFile | ReadFile | WriteFile | DBReady; }
    virtual Qn::StorageInitResult initOrUpdate() override { return Qn::StorageInit_Ok; }
    virtual void setUrl(const QString& url) override { this->url = url; }
    virtual QString getUrl() const override { return url; }
    virtual bool isSystem() const override { return isSystemFlag; }
    virtual QString getPath() const override { return QnStorageResource::getPath(); }
    virtual bool isOnline() const override { return isOnlineFlag; }
};

class StorageManagerStub: public QnStorageManager
{
public:
    nx::vms::server::StorageResourceList storages;
    std::chrono::seconds systemFreeSpaceInterval{1};

    using QnStorageManager::QnStorageManager;

private:
    virtual std::chrono::milliseconds checkSystemFreeSpaceInterval() const override { return systemFreeSpaceInterval; }
    virtual StorageResourceList getStorages() const override { return storages; }
};

/**
 * Adds a storage fixture. It will be seen by the storage manager but won't perform any real actions
 * like reading, writing files.
 */
StorageResourcePtr addStorageStub(
    MediaServerLauncher* server, QnMediaServerModule* serverModule, const QString& url,
    int64_t totalSpace, int64_t freeSpace, int64_t spaceLimit, bool isSystem, bool isOnline,
    bool isUsedForWriting);

void setNxOccupiedSpace(
    MediaServerLauncher* server, const QnStorageResourcePtr& storage, int64_t nxOccupiedSpace);

struct ChunkData
{
    qint64 startTimeMs;
    qint64 durationsMs;
    QString path;
};

using ChunkDataList = std::vector<ChunkData>;

struct Catalog
{
    ChunkDataList lowQualityChunks;
    ChunkDataList highQualityChunks;
};

using Archive = QMap<QString, Catalog>;

static const int kHiQualityFileWidth = 640;
static const int kHiQualityFileHeight = 480;
static const int kLowQualityFileWidth = 320;
static const int kLowQualityFileHeight = 240;
static const int64_t kMediaFileDurationMs = 60000;
static const int64_t kMediaFilesGapMs = 40;

Catalog generateCameraArchive(
    const QString& baseDir, const QString& cameraName, qint64 startTimeMs, int count);

QByteArray createTestMkvFileData(
    int lengthSec, int width, int height, const QString& container = "matroska",
    const QString& codec = "h263p");

void updateMkvMetaData(QByteArray& payload, int64_t startTimeMs);

std::unique_ptr<QnArchiveStreamReader> createArchiveStreamReader(
    const QnVirtualCameraResourcePtr& camera);

ChunkDataList generateChunksForQuality(const QString& baseDir, const QString& cameraName,
    const QString& quality, qint64 startTimeMs, int durationMs, int count, QByteArray& payload);

QnVirtualCameraResourcePtr getCamera(QnResourcePool* resourcePool, const QString& uniqueIdOrName);

/**
 * Checks that media data played covers the given time periods. This function waits until
 * above happens or forever.
 */
void checkPlaybackCorrecteness(
    QnArchiveStreamReader* archiveReader, const QnTimePeriodList& expectedTimePeriods,
    int64_t mediaFileDurationMs, int64_t mediaFileGapMs, bool shouldSucceed = true);

} // namespace nx::vms::server::test::test_support
