#pragma once

class MediaServerLauncher;

namespace nx::test_support {

void addTestStorage(MediaServerLauncher* server, const QString& path);

struct ChunkData
{
    qint64 startTimeMs;
    qint64 durationsMs;
};

using ChunkDataList = std::vector<ChunkData>;

struct Catalog
{
    ChunkDataList lowQualityChunks;
    ChunkDataList highQualityChunks;
};

using Archive = QMap<QString, Catalog>;

Catalog generateCameraArchive(
    const QString& baseDir,const QString& cameraName, qint64 startTimeMs, int count);

} // namespace nx::test_support