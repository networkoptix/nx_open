#include <memory>

#include <nx/kit/test.h>

#include <tegra_video.h>

static std::unique_ptr<TegraVideo> tegraVideo;

TEST(tegra_video_stub, setup)
{
    tegraVideo.reset(TegraVideo::createStub());
}

TEST(tegra_video_stub, start)
{
    TegraVideo::Params params;
    params.id = "test_id";
    params.modelFile = "test_modelFile";
    params.deployFile = "test_deployFile";
    params.cacheFile = "test_cacheFile";
    params.netWidth = 0;
    params.netHeight= 0;

    ASSERT_TRUE(tegraVideo->start(params));
}

TEST(tegra_video_stub, frames)
{
    ASSERT_FALSE(tegraVideo->hasMetadata());

    const TegraVideo::CompressedFrame compressedFrame{};
    ASSERT_TRUE(tegraVideo->pushCompressedFrame(&compressedFrame));

    ASSERT_TRUE(tegraVideo->hasMetadata());

    static constexpr int kUntouchedValue = -113;

    static constexpr int kMaxRectsCount = 2;
    TegraVideo::Rect rects[kMaxRectsCount];
    rects[0].x = kUntouchedValue;
    rects[0].y = kUntouchedValue;
    rects[0].width = kUntouchedValue;
    rects[0].height = kUntouchedValue;
    rects[1].x = kUntouchedValue;
    rects[1].y = kUntouchedValue;
    rects[1].width = kUntouchedValue;
    rects[1].height = kUntouchedValue;

    int rectsCount = kUntouchedValue;
    int64_t ptsUs = kUntouchedValue;

    ASSERT_TRUE(tegraVideo->pullRectsForFrame(rects, kMaxRectsCount, &rectsCount, &ptsUs));

    ASSERT_EQ(rectsCount, 2); //< Stub is expected to produce a two rects.
    ASSERT_EQ(ptsUs, 0);
    ASSERT_TRUE(rects[0].x >= 0);
    ASSERT_TRUE(rects[0].y >= 0);
    ASSERT_TRUE(rects[0].width > 0);
    ASSERT_TRUE(rects[0].height > 0);
    ASSERT_TRUE(rects[1].x >= 0);
    ASSERT_TRUE(rects[1].y >= 0);
    ASSERT_TRUE(rects[1].width > 0);
    ASSERT_TRUE(rects[1].height > 0);
}

TEST(tegra_video_stub, stop)
{
    ASSERT_TRUE(tegraVideo->stop());
}
