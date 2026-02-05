// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <fstream>

#include <nx/webrtc/webrtc_datachannel.h>
#include <nx/utils/log/log.h>

using namespace nx::webrtc;

class TestDataChannel;

enum Type
{
    client,
    server
};

class TestDataChannelStreamer: public AbstractDataChannelDelegate
{
public:
    TestDataChannelStreamer(TestDataChannel* testDataChannel, Type type)
        :
        AbstractDataChannelDelegate(SessionConfig()),
        m_testDataChannel(testDataChannel),
        m_type(type)
    {}
    virtual ~TestDataChannelStreamer()
    {
        m_dataChannel.reset();
    }
    virtual void writeDataChannelPacket(const uint8_t* data, int size);
    virtual void onDataChannelString(const std::string& data, int /*streamId*/)
    {
        m_lastString = data;
    }
    virtual void onDataChannelBinary(const std::string& data, int streamId)
    {
        onDataChannelString(data, streamId);
    }
    bool openDataChannel()
    {
        return m_dataChannel.openDataChannel("test-channel", "");
    }
    bool writeString(const std::string& string)
    {
        return m_dataChannel.writeString(string);
    }
    bool handlePacket(const uint8_t* data, int size)
    {
        return m_dataChannel.handlePacket(data, size);
    }
    std::string lastString() const { return m_lastString; }
private:
    TestDataChannel* m_testDataChannel = nullptr;
    Type m_type = Type::client;
    std::string m_lastString;
};

class TestDataChannel
{
public:
    TestDataChannel()
        :
        m_streamerClient(this, Type::client),
        m_streamerServer(this, Type::server)
    {}
    bool writePacket(const uint8_t* data, int size, Type type);
    bool flushQueues();
    bool test();
private:
    TestDataChannelStreamer m_streamerClient;
    TestDataChannelStreamer m_streamerServer;
    std::queue<std::vector<uint8_t>> m_clientServerQueue;
    std::queue<std::vector<uint8_t>> m_serverClientQueue;
};

/*virtual*/ void TestDataChannelStreamer::writeDataChannelPacket(const uint8_t* data, int size)
{
    m_testDataChannel->writePacket(data, size, m_type);
}

bool TestDataChannel::writePacket(const uint8_t* data, int size, Type type)
{
    // Input of client Streamer is output of server Streamer, and vice versa.
    // But you can't write directly because of deadlock in library,
    // so, emulating asynchronous input/output with queues.
    if (type == Type::server)
    {
        m_serverClientQueue.emplace(data, &data[size]);
    }
    else
    {
        m_clientServerQueue.emplace(data, &data[size]);
    }
    return true;
}

bool TestDataChannel::flushQueues()
{
    while (!m_clientServerQueue.empty() || !m_serverClientQueue.empty())
    {
        auto flush = [](std::queue<std::vector<uint8_t>>& queue, TestDataChannelStreamer& streamer)
        {
            while (!queue.empty())
            {
                auto packet = std::move(queue.front());
                queue.pop();
                if (!streamer.handlePacket(packet.data(), packet.size()))
                    return false;
            }
            return true;
        };
        if (!flush(m_serverClientQueue, m_streamerClient)
            || !flush(m_clientServerQueue, m_streamerServer))
            return false;
    }
    return true;
}

bool TestDataChannel::test()
{
    if (!m_streamerClient.openDataChannel())
        return false;
    if (!flushQueues())
        return false;

    if (!m_streamerServer.writeString("hello"))
        return false;
    if (!flushQueues())
        return false;

    if (m_streamerClient.lastString() != "hello")
        return false;

    if (!m_streamerClient.writeString("world"))
        return false;
    if (!flushQueues())
        return false;

    if (m_streamerServer.lastString() != "world")
        return false;

    if (!m_streamerServer.writeString("lorem"))
        return false;
    if (!flushQueues())
        return false;

    if (m_streamerClient.lastString() != "lorem")
        return false;

    if (!m_streamerClient.writeString("ipsum"))
        return false;
    if (!flushQueues())
        return false;

    if (m_streamerServer.lastString() != "ipsum")
        return false;

    return true;
}

TEST(WebRTC, DataChannel)
{
    TestDataChannel test;
    ASSERT_TRUE(test.test());
}
