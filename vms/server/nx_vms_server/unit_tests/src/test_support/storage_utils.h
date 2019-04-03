#pragma once

#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>

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

QnVirtualCameraResourcePtr cameraByUniqueId(QnResourcePool* resourcePool, const QString& uniqueId);

/**
 * Checks that media data played covers the given time periods. The wait() function hangs until
 * above happens.
 */
class PlaybackChecker: public QnAbstractMediaDataReceptor
{
public:
    PlaybackChecker(const QnTimePeriodList& timePeriods);

    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    void wait();
    void reset();

private:
    QnMutex m_archivePlaybackMutex;
    QnWaitCondition m_archivePlaybackWaitCondition;
    bool m_archivePlayedTillTheEnd = false;
    QnTimePeriodList m_timePeriods;
    int64_t m_timeGap = 0;
};

} // namespace nx::vms::server::test::test_support