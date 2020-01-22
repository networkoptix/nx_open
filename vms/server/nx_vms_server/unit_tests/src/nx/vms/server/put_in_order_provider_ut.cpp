#include <gtest/gtest.h>

#include <nx/vms/server/put_in_order_data_provider.h>
#include <nx/streaming/video_data_packet.h>

using namespace nx::utils::test;
using namespace std::chrono;

namespace nx::vms::server::test {

TEST(SimpleReorderer, zeroInitialBufferDesc)
{
    SimpleReorderer reorderer;

    for (int i = 10; i > 0; --i)
    {
        QnAbstractDataPacketPtr data(new QnWritableCompressedVideoData());
        data->timestamp = i;
        reorderer.processNewData(data);
    }

    // First packet unordered, after buffer have been automatically increased.
    auto& queue = reorderer.queue();
    ASSERT_EQ(1, queue.size());
    ASSERT_EQ(10, queue.front()->timestamp);

    queue.clear();
    reorderer.flush();

    // All other packets are ordered.
    ASSERT_EQ(9, queue.size());
    for (int i = 1; i <= 9; ++i)
    {
        ASSERT_EQ(i, queue.front()->timestamp);
        queue.pop_front();
    }
}

TEST(SimpleReorderer, zeroInitialBufferAsc)
{
    SimpleReorderer reorderer;
    for (int i = 1; i <= 10; ++i)
    {
        QnAbstractDataPacketPtr data(new QnWritableCompressedVideoData());
        data->timestamp = i;
        reorderer.processNewData(data);
    }

    auto& queue = reorderer.queue();
    ASSERT_EQ(10, queue.size());
    for (int i = 1; i <= 10; ++i)
    {
        ASSERT_EQ(i, queue.front()->timestamp);
        queue.pop_front();
    }
}

TEST(SimpleReorderer, nonZeroInitialBuffer)
{
    SimpleReorderer reorderer({1s});
    // All packets should be reordered.
    for (int i = 10; i > 0; --i)
    {
        QnAbstractDataPacketPtr data(new QnWritableCompressedVideoData());
        data->timestamp = i;
        reorderer.processNewData(data);
    }

    auto& queue = reorderer.queue();
    reorderer.flush();
    ASSERT_EQ(10, queue.size());
    for (int i = 1; i <= 10; ++i)
    {
        ASSERT_EQ(i, queue.front()->timestamp);
        queue.pop_front();
    }
}

TEST(SimpleReorderer, bufferSize)
{
    SimpleReorderer reorderer({10us});
    // After buffer has been filled up, packet should be available without 'flush' call
    for (int i = 1; i <= 15; ++i)
    {
        QnAbstractDataPacketPtr data(new QnWritableCompressedVideoData());
        data->timestamp = i;
        reorderer.processNewData(data);
    }

    auto& queue = reorderer.queue();
    ASSERT_EQ(5, queue.size());
    for (int i = 1; i <= 5; ++i)
    {
        ASSERT_EQ(i, queue.front()->timestamp);
        queue.pop_front();
    }
}

} // namespace nx:vms::server::test
