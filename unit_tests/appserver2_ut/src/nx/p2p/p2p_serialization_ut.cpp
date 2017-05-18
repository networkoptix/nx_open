#include <gtest/gtest.h>

#include <utils/media/bitStream.h>
#include <nx/p2p/p2p_serialization.h>

namespace nx {
namespace p2p {
namespace test {

TEST(P2pMessageBus, CompressPeerNumber)
{
    using namespace ec2;

    QVector<PeerNumberType> peers;
    for (int i = 0; i < kMaxOnlineDistance; ++i)
        peers.push_back(i);
    QVector<PeerNumberType> peers2;

    try
    {

        // serialize
        QByteArray serializedData;
        serializedData.resize(peers.size() * 2);
        BitStreamWriter writer;
        writer.setBuffer((quint8*)serializedData.data(), serializedData.size());
        for (const auto& peer : peers)
            serializeCompressPeerNumber(writer, peer);
        writer.flushBits();
        serializedData.truncate(writer.getBytesCount());

        // deserialize back
        BitStreamReader reader((const quint8*)serializedData.data(), serializedData.size());
        while (reader.bitsLeft() >= 8)
            peers2.push_back(deserializeCompressPeerNumber(reader));
    }
    catch (...)
    {
        ASSERT_TRUE(0);
    }
    ASSERT_EQ(peers, peers2);
}

TEST(P2pMessageBus, CompressPeerNumber2)
{
    using namespace ec2;

    try
    {
        auto writeData = [](QByteArray& buffer, int filler)
        {
            buffer.resize(4);
            buffer[0] = buffer[1] = filler;
            BitStreamWriter writer;
            writer.setBuffer((quint8*)buffer.data(), buffer.size());
            serializeCompressPeerNumber(writer, 1023);
            writer.flushBits(true);
            buffer.truncate(2);
        };

        // serialize
        QByteArray data1;
        QByteArray data2;
        writeData(data1, 0);
        writeData(data2, 0xff);
        ASSERT_EQ(data1, data2);
    }
    catch (...)
    {
        ASSERT_TRUE(0);
    }
}

TEST(P2pMessageBus, PeersMessage)
{
    using namespace nx::p2p;

    QVector<PeerDistanceRecord> peers;
    for (int i = 0; i < 100; ++i)
        peers.push_back(PeerDistanceRecord(i, i * 1000));

    QByteArray serializedData = serializePeersMessage(peers);
    bool success = false;
    auto deserializedPeers = deserializePeersMessage(serializedData, &success);
    QByteArray serializedData2 = serializePeersMessage(deserializedPeers);
    ASSERT_TRUE(success);
    ASSERT_EQ(serializedData, serializedData2);
}

} // namespace test
} // namespace p2p
} // namespace nx
