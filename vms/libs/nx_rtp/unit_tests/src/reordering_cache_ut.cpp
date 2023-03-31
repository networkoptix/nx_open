// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/reordering_cache.h>

using namespace nx::rtp;

ReorderingCache::Status pushFakePacket(ReorderingCache& cache, uint16_t seq)
{
    std::string toWrite = "$" + std::to_string(seq);
    nx::utils::ByteArray data;
    data.write(toWrite.data(), toWrite.size());
    return cache.pushPacket(data, seq);
}

std::string flushToString(ReorderingCache& cache)
{
    std::string output;
    bool hasData = false;
    nx::utils::ByteArray packet;
    while (cache.getNextPacket(packet))
    {
        output.append(packet.data(), packet.size());
        hasData = true;
        packet.clear();
    }
    if (!hasData)
        output = "-";
    return output;
}

std::optional<RtcpNackReport> buildFakeReport(auto... args)
{
    std::vector<uint16_t> seqs{static_cast<uint16_t>(args)...};
    if (seqs.empty())
        return std::nullopt;
    return buildNackReport(0, 0, seqs);
}

TEST(RtpReorderingCache, readWrite)
{
    using Status = ReorderingCache::Status;

    // Always pass for small cache.
    {
        ReorderingCache cache(1);
        ASSERT_EQ(pushFakePacket(cache, 1), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 2), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 10), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 1), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 0), Status::pass);
        ASSERT_EQ(flushToString(cache), "-");
    }

    // Too big distance.
    {
        ReorderingCache cache(2);
        ASSERT_EQ(pushFakePacket(cache, 1), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 2), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 10), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 1), Status::skip);
        ASSERT_EQ(pushFakePacket(cache, 0), Status::skip);
        ASSERT_EQ(flushToString(cache), "-");
    }
    // Normal case.
    {
        ReorderingCache cache(2);
        ASSERT_EQ(pushFakePacket(cache, 1), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 2), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 3), Status::pass);
        ASSERT_EQ(flushToString(cache), "-");
    }
    // Normal drop.
    {
        ReorderingCache cache(2);
        ASSERT_EQ(pushFakePacket(cache, 1), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 2), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 4), Status::wait);
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport(3));
        ASSERT_EQ(flushToString(cache), "-");
        ASSERT_EQ(pushFakePacket(cache, 3), Status::flush);
        ASSERT_EQ(flushToString(cache), "$3$4");
    }
    // Mixed drop #1.
    {
        ReorderingCache cache(2);
        ASSERT_EQ(pushFakePacket(cache, 1), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 2), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 4), Status::wait);
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport(3));
        ASSERT_EQ(flushToString(cache), "-");
        ASSERT_EQ(pushFakePacket(cache, 1), Status::skip);
        ASSERT_EQ(pushFakePacket(cache, 2), Status::skip);
        ASSERT_EQ(pushFakePacket(cache, 4), Status::wait);
        ASSERT_EQ(pushFakePacket(cache, 3), Status::flush);
        ASSERT_EQ(flushToString(cache), "$3$4");
    }
    // Mixed drop #2.
    {
        ReorderingCache cache(5);
        ASSERT_EQ(pushFakePacket(cache, 1), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 2), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 4), Status::wait);
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport(3));
        ASSERT_EQ(flushToString(cache), "-");

        ASSERT_EQ(pushFakePacket(cache, 1), Status::skip);
        ASSERT_EQ(pushFakePacket(cache, 2), Status::skip);
        ASSERT_EQ(pushFakePacket(cache, 4), Status::wait);
        ASSERT_EQ(pushFakePacket(cache, 6), Status::wait);
        ASSERT_EQ(pushFakePacket(cache, 7), Status::wait);
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport(3, 5));
        ASSERT_EQ(flushToString(cache), "-");

        ASSERT_EQ(pushFakePacket(cache, 3), Status::flush);
        ASSERT_EQ(flushToString(cache), "$3$4");
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport(5));

        ASSERT_EQ(pushFakePacket(cache, 7), Status::wait);
        ASSERT_EQ(pushFakePacket(cache, 3), Status::skip);
        ASSERT_EQ(pushFakePacket(cache, 5), Status::flush);
        ASSERT_EQ(flushToString(cache), "$5$6$7");

        ASSERT_EQ(pushFakePacket(cache, 8), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 10), Status::wait);
        ASSERT_EQ(pushFakePacket(cache, 12), Status::wait);
        ASSERT_EQ(pushFakePacket(cache, 14), Status::flush); //< seq 9 is popped.
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport(11, 13));
        ASSERT_EQ(flushToString(cache), "$10");

        ASSERT_EQ(pushFakePacket(cache, 9), Status::skip);
        ASSERT_EQ(pushFakePacket(cache, 11), Status::flush);
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport(13));
        ASSERT_EQ(flushToString(cache), "$11$12");

        ASSERT_EQ(pushFakePacket(cache, 13), Status::flush);
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport());
        ASSERT_EQ(flushToString(cache), "$13$14");

        ASSERT_EQ(pushFakePacket(cache, 32768), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 65534), Status::pass);
        ASSERT_EQ(pushFakePacket(cache, 0), Status::wait);
        ASSERT_EQ(cache.getNextNackPacket(0, 0), buildFakeReport(65535));

        ASSERT_EQ(pushFakePacket(cache, 1), Status::wait);
        ASSERT_EQ(pushFakePacket(cache, 65535), Status::flush);
        ASSERT_EQ(flushToString(cache), "$65535$0$1");
    }
}
