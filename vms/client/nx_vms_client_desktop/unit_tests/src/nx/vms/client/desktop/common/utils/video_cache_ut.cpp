// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <nx/vms/client/desktop/utils/video_cache.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace std::chrono;

CLVideoDecoderOutputPtr makeFrame(std::chrono::microseconds timestampUs)
{
    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    result->pkt_dts = timestampUs.count();
    return result;
}

TEST(videoCache, cacheSize)
{
    VideoCache cache;
    cache.setCacheSize(1s);
    nx::Uuid id = nx::Uuid::createUuid();
    cache.setCachedDevices(0, QSet<nx::Uuid>{id});
    cache.add(id, makeFrame(1ms));
    cache.add(id, makeFrame(800ms));
    cache.add(id, makeFrame(100ms));

    microseconds result;
    cache.image(id, 0ms, &result);
    ASSERT_EQ(result, 1ms);
    cache.image(id, 100ms, &result);
    ASSERT_EQ(result, 100ms);
    cache.image(id, 700ms, &result);
    ASSERT_EQ(result, 800ms);


    cache.add(id, makeFrame(1001ms));
    cache.image(id, 0ms, &result);
    ASSERT_EQ(result, 100ms);
}

TEST(videoCache, resourceList)
{
    VideoCache cache;
    cache.setCacheSize(1s);
    nx::Uuid id1 = nx::Uuid::createUuid();
    nx::Uuid id2 = nx::Uuid::createUuid();
    nx::Uuid id3 = nx::Uuid::createUuid();

    cache.setCachedDevices(0, QSet<nx::Uuid>{id1, id2});

    cache.add(id1, makeFrame(1ms));
    cache.add(id2, makeFrame(1ms));
    cache.add(id3, makeFrame(1ms));

    microseconds result;
    cache.image(id1, 0ms, &result);
    ASSERT_EQ(result, 1ms);
    cache.image(id2, 0ms, &result);
    ASSERT_EQ(result, 1ms);
    cache.image(id3, 0ms, &result);
    ASSERT_EQ(result, VideoCache::kNoTimestamp);

    cache.setCachedDevices(0, QSet<nx::Uuid>{id2, id3});
    cache.add(id3, makeFrame(1ms));

    cache.image(id1, 0ms, &result);
    ASSERT_EQ(result, VideoCache::kNoTimestamp);
    cache.image(id2, 0ms, &result);
    ASSERT_EQ(result, 1ms);
    cache.image(id3, 0ms, &result);
    ASSERT_EQ(result, 1ms);
}

} // namespace test
} // namespace nx::vms::client::desktop
