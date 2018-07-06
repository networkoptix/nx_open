#include <gtest/gtest.h>

#include <chrono>

#include <nx/streaming/video_data_packet.h>
#include <nx/utils/random.h>
#include <nx/mediaserver/hls/hls_live_playlist_manager.h>
#include <utils/media/media_stream_cache.h>
#include <media_server/settings.h>

TEST(hls_LivePlaylistManager, general)
{
    using namespace std::chrono;

    const microseconds targetDuration(5*1000*1000);
    const microseconds frameStep(30*1000);
    const int gopSizeFrames = 15;
    const hours testDuration(1);
    const milliseconds targetCacheSize = seconds(10);
    const milliseconds maxCacheSize = std::chrono::duration_cast<milliseconds>(targetDuration)*5;

    MediaStreamCache mediaCache(
        targetCacheSize.count(),
        maxCacheSize.count());
    nx::mediaserver::hls::LivePlaylistManager hlsPlaylistManager(
        &mediaCache,
        targetDuration.count(),
        nx_ms_conf::DEFAULT_HLS_REMOVED_LIVE_CHUNKS_TO_KEEP);

    microseconds totalTimestampChange(0);
    const microseconds startTimestamp(nx::utils::random::number());
    int curGopSize = gopSizeFrames;

    for (microseconds curTimestamp = startTimestamp;
         curTimestamp - startTimestamp < testDuration;
         curTimestamp += frameStep)
    {
        //generating input data
        auto packet = std::make_shared<QnWritableCompressedVideoData>();
        packet->timestamp = curTimestamp.count();
        if (curGopSize >= gopSizeFrames)
        {
            packet->flags |= QnAbstractMediaData::MediaFlags_AVKey;
            curGopSize = 0;
        }
        else
        {
            ++curGopSize;
        }

        mediaCache.putData(std::move(packet));

        //checking that hlsPlaylistManager refers to existing data
        std::vector<nx::mediaserver::hls::AbstractPlaylistManager::ChunkData> chunks;
        bool endOfStreamReached = false;
        hlsPlaylistManager.generateChunkList(&chunks, &endOfStreamReached);
        ASSERT_FALSE(endOfStreamReached);

        if (chunks.empty())
        {
            ASSERT_TRUE(totalTimestampChange < targetDuration*2);
        }
        else
        {
            ASSERT_TRUE(chunks.front().startTimestamp >= mediaCache.startTimestamp());
            ASSERT_TRUE(
                chunks.back().startTimestamp + chunks.back().duration <=
                mediaCache.currentTimestamp());
            quint64 foundTimestamp = 0;
            const auto foundPacket = mediaCache.findByTimestamp(
                chunks.front().startTimestamp, true, &foundTimestamp);
            ASSERT_TRUE(foundPacket != nullptr);
            ASSERT_EQ(chunks.front().startTimestamp, foundPacket->timestamp);
            ASSERT_TRUE((std::static_pointer_cast<QnAbstractMediaData>(foundPacket)->flags & QnAbstractMediaData::MediaFlags_AVKey) > 0);
        }
    }
}
