#include <memory>

#include <nx/kit/test.h>

#include <tegra_video_stub.h>

static std::unique_ptr<TegraVideo, decltype(&tegraVideoDestroy)> tegraVideo{
    nullptr, tegraVideoDestroy};

TEST(tegra_video_stub, setup)
{
    tegraVideo.reset(tegraVideoCreateStub());
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

    ASSERT_TRUE(tegraVideo->start(&params));
}

void testRect(const TegraVideo::Rect& rect)
{
    ASSERT_TRUE(rect.x >= 0);
    ASSERT_TRUE(rect.x <= 1);
    ASSERT_TRUE(rect.y >= 0);
    ASSERT_TRUE(rect.y <= 1);
    ASSERT_TRUE(rect.w > 0);
    ASSERT_TRUE(rect.h > 0);
    ASSERT_TRUE(rect.x + rect.w <= 1);
    ASSERT_TRUE(rect.y + rect.h <= 1);
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
    rects[0].w = kUntouchedValue;
    rects[0].h = kUntouchedValue;
    rects[1].x = kUntouchedValue;
    rects[1].y = kUntouchedValue;
    rects[1].w = kUntouchedValue;
    rects[1].h = kUntouchedValue;

    int rectsCount = kUntouchedValue;
    int64_t ptsUs = kUntouchedValue;

    ASSERT_TRUE(tegraVideo->pullRectsForFrame(rects, kMaxRectsCount, &rectsCount, &ptsUs));

    ASSERT_EQ(rectsCount, 2); //< Stub is expected to produce a two rects.
    ASSERT_EQ(ptsUs, 0);

    for (int i = 0; i < rectsCount; ++i)
        testRect(rects[i]);
}

TEST(tegra_video_stub, stop)
{
    ASSERT_TRUE(tegraVideo->stop());
}
