#pragma once

#include <core/resource/resource_fwd.h>

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
QnStorageResourcePtr addStorageFixture(
    MediaServerLauncher* server, QnMediaServerModule* serverModule, const QString& url,
    int64_t totalSpace, int64_t freeSpace, int64_t spaceLimit, bool isSystem, bool isOnline);

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

Catalog generateCameraArchive(
    const QString& baseDir, const QString& cameraName, qint64 startTimeMs, int count);

QByteArray createTestMkvFile(int lengthSec, int width, int height);
std::unique_ptr<QnArchiveStreamReader> createArchiveStreamReader(
    const QnVirtualCameraResourcePtr& camera);

} // namespace nx::vms::server::test::test_support
