#include <gtest/gtest.h>
#include <nx/network/websocket/websocket_multibuffer.h>


namespace nx {
namespace network {
namespace websocket {

namespace test {

class WebsocketMultibuffer:
    public ::testing::Test
{
protected:
    enum class Locked
    {
        yes,
        no
    };

    const int kPerBufferWrites = 5;
    const char* const kPattern = "hello";

    void assertCorrectEmptyState()
    {
        ASSERT_EQ(buffer.readySize(), 0);
        ASSERT_EQ(buffer.locked(), false);
        ASSERT_EQ(buffer.popFront(), nx::Buffer());

        // assert no crashes
        buffer.clearLast();
        buffer.lock();
    }

    void whenSomeBuffersAppended(int bufferCount, int perBufferWrites, Locked lockLast)
    {
        for (int i = 0; i < bufferCount; ++i)
        {
            for (int j = 0; j < perBufferWrites; ++j)
                buffer.append(kPattern);
            if (i != bufferCount - 1 || lockLast == Locked::yes)
                buffer.lock();
        }
    }

    MultiBuffer buffer;
};

TEST_F(WebsocketMultibuffer, InitialState)
{
    assertCorrectEmptyState();
}

TEST_F(WebsocketMultibuffer, SingleBuffer_lockLast)
{
    whenSomeBuffersAppended(1, kPerBufferWrites, Locked::yes);
    ASSERT_EQ(buffer.readySize(), 1);

    auto popResult = buffer.popFront();
    ASSERT_EQ(popResult.size(), kPerBufferWrites * (int)strlen(kPattern));

    assertCorrectEmptyState();
}

TEST_F(WebsocketMultibuffer, SingleBuffer_noLockLast)
{
    whenSomeBuffersAppended(1, kPerBufferWrites, Locked::no);
    assertCorrectEmptyState();
}

TEST_F(WebsocketMultibuffer, MultipleBuffer_lockLast)
{
    whenSomeBuffersAppended(10, kPerBufferWrites, Locked::yes);
    ASSERT_EQ(buffer.readySize(), 10);

    auto popResult = buffer.popFront();
    ASSERT_EQ(popResult.size(), kPerBufferWrites * (int)strlen(kPattern));
    ASSERT_EQ(buffer.readySize(), 9);
}

TEST_F(WebsocketMultibuffer, MultipleBuffer_noLockLast)
{
    whenSomeBuffersAppended(10, kPerBufferWrites, Locked::no);
    ASSERT_EQ(buffer.readySize(), 9);
}

TEST_F(WebsocketMultibuffer, clearLast)
{
    whenSomeBuffersAppended(1, kPerBufferWrites, Locked::yes);
    ASSERT_EQ(buffer.readySize(), 1);

    buffer.clearLast();
    assertCorrectEmptyState();
}

TEST_F(WebsocketMultibuffer, clear)
{
    whenSomeBuffersAppended(10, kPerBufferWrites, Locked::yes);

    buffer.clear();
    assertCorrectEmptyState();
}

} // namespace test
} // namespace network
} // namespace websocket
} // namespace nx