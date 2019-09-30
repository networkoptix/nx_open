#pragma once

#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <recording/time_period_list.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/vms/server/resource/storage_resource.h>

class MediaServerLauncher;
class QnMediaServerModule;
class QnArchiveStreamReader;

namespace nx::vms::server::test::test_support {

/**
 * Adds a real storage which can be used for storing files, reindexing e.t.c.
 */
void addStorage(MediaServerLauncher* server, const QString& path);

/**
 * Adds a storage fixture. It will be seen by the storage manager but won't perform any real actions
 * like reading, writing files.
 */
StorageResourcePtr addStorageFixture(
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

QnVirtualCameraResourcePtr getCamera(QnResourcePool* resourcePool, const QString& uniqueIdOrName);

/**
 * Checks that media data played covers the given time periods. This function waits until
 * above happens or forever.
 */
void checkPlaybackCorrecteness(
    QnArchiveStreamReader* archiveReader, const QnTimePeriodList& expectedTimePeriods,
    int64_t mediaFileDurationMs, int64_t mediaFileGapMs, bool shouldSucceed = true);

} // namespace nx::vms::server::test::test_support
