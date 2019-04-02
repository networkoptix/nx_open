#pragma once

#include <core/resource/resource_fwd.h>

class MediaServerLauncher;
class QnArchiveStreamReader;

namespace nx::vms::server::test::test_support {

void addTestStorage(MediaServerLauncher* server, const QString& path);

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