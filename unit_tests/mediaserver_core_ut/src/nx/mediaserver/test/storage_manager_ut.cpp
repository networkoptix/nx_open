#include <gtest/gtest.h>

#include <recorder/device_file_catalog.h>
#include <recorder/storage_manager.h>
#include <nx/core/access/access_types.h>
#include <server/server_globals.h>
#include <common/common_module.h>

#include "media_server_module_fixture.h"

namespace nx {
namespace mediaserver {
namespace test {

void addChunk(
    std::deque<DeviceFileCatalog::Chunk>& chunks,
    qint64 startTimeMs,
    qint64 durationMs)
{
    DeviceFileCatalog::Chunk chunk;
    chunk.startTimeMs = startTimeMs;
    chunk.durationMs = durationMs;
    chunks.push_back(chunk);
};

class StorageManager:
    public MediaServerModuleFixture
{
};

TEST_F(StorageManager, deleteRecordsToTime)
{
    std::deque<DeviceFileCatalog::Chunk> chunks;
    addChunk(chunks, 5, 10);
    addChunk(chunks, 20, 5);
    addChunk(chunks, 100, 500);

    DeviceFileCatalogPtr catalog(new DeviceFileCatalog(
        lit("camera1"),
        QnServer::ChunksCatalog::HiQualityCatalog,
        QnServer::StoragePool::Normal));
    catalog->addChunks(chunks);

    qnNormalStorageMan->deleteRecordsToTime(catalog, 4);
    ASSERT_EQ(5, catalog->minTime());

    qnNormalStorageMan->deleteRecordsToTime(catalog, 50);
    ASSERT_EQ(100, catalog->minTime());

    qnNormalStorageMan->deleteRecordsToTime(catalog, 100);
    ASSERT_EQ(100, catalog->minTime());

    qnNormalStorageMan->deleteRecordsToTime(catalog, 599);
    ASSERT_EQ(100, catalog->minTime());

    qnNormalStorageMan->deleteRecordsToTime(catalog, 600);
    ASSERT_EQ(AV_NOPTS_VALUE, catalog->minTime());

    qnNormalStorageMan->stopAsyncTasks();
    qnBackupStorageMan->stopAsyncTasks();
}

} // namespace test
} // namespace mediaserver
} // namespace nx
