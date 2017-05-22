#include <gtest/gtest.h>

#include <utils/media/bitStream.h>
#include <nx/p2p/p2p_serialization.h>

namespace nx {
namespace p2p {
namespace test {

TEST(P2pSerialization, WriterFlushBits)
{
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
        ASSERT_EQ(data1.toHex(), data2.toHex());
    }
    catch (...)
    {
        ASSERT_TRUE(0);
    }
}

TEST(P2pSerialization, CompressedPeers)
{
    QVector<PeerNumberType> peers;
    for (int i = 0; i < kMaxOnlineDistance; ++i)
        peers.push_back(i);

    QByteArray serializedData = serializeCompressedPeers(peers, 0);
    bool success = false;
    auto peers2 = deserializeCompressedPeers(serializedData, &success);
    ASSERT_TRUE(success);
    ASSERT_EQ(peers, peers2);
}

TEST(P2pSerialization, CompressedSize)
{
    QVector<quint32> values;
    for (int i = 0; i < 1000000 * 500; i += 1000000)
        values.push_back(i);
    QVector<quint32> values2;

    try
    {
        QByteArray serializedData;
        serializedData.resize(values.size() * 4);
        BitStreamWriter writer;
        writer.setBuffer((quint8*)serializedData.data(), serializedData.size());
        for (const auto& value: values)
            serializeCompressedSize(writer, value);
        writer.flushBits();
        serializedData.truncate(writer.getBytesCount());

        // deserialize back
        BitStreamReader reader((const quint8*) serializedData.data(), serializedData.size());
        while (reader.bitsLeft() >= 8)
            values2.push_back(deserializeCompressedSize(reader));
    }
    catch (...)
    {
        ASSERT_TRUE(0);
    }
    ASSERT_EQ(values, values2);
}

TEST(P2pSerialization, PeersMessage)
{
    using namespace nx::p2p;

    QVector<PeerDistanceRecord> peers;
    for (int i = 0; i < 100; ++i)
        peers.push_back(PeerDistanceRecord(i, i * 1000));

    QByteArray serializedData = serializePeersMessage(peers, 0);
    bool success = false;
    auto deserializedPeers = deserializePeersMessage(serializedData, &success);
    QByteArray serializedData2 = serializePeersMessage(deserializedPeers, 0);
    ASSERT_TRUE(success);
    ASSERT_EQ(serializedData.toHex(), serializedData2.toHex());
}

TEST(P2pSerialization, SubscribeRequest)
{
    using namespace nx::p2p;

    QVector<SubscribeRecord> peers;
    for (int i = 0; i < 100; ++i)
        peers.push_back(SubscribeRecord(i, i * 1000));

    QByteArray serializedData = serializeSubscribeRequest(peers, 0);
    bool success = false;
    auto deserializedPeers = deserializeSubscribeRequest(serializedData, &success);
    QByteArray serializedData2 = serializeSubscribeRequest(deserializedPeers, 0);
    ASSERT_TRUE(success);
    ASSERT_EQ(serializedData.toHex(), serializedData2.toHex());
}

TEST(P2pSerialization, ResolvePeerNumberResponse)
{
    using namespace nx::p2p;

    QVector<PeerNumberResponseRecord> peers;
    for (int i = 0; i < 100; ++i)
    {
        ec2::ApiPersistentIdData id(QnUuid::createUuid(), QnUuid::createUuid());
        peers.push_back(PeerNumberResponseRecord(i, id));
    }

    QByteArray serializedData = serializeResolvePeerNumberResponse(peers, 0);
    bool success = false;
    auto deserializedPeers = deserializeResolvePeerNumberResponse(serializedData, &success);
    QByteArray serializedData2 = serializeResolvePeerNumberResponse(deserializedPeers, 0);
    ASSERT_TRUE(success);
    ASSERT_EQ(serializedData.toHex(), serializedData2.toHex());
}

TEST(P2pSerialization, TransactionList)
{
    using namespace nx::p2p;

    QList<QByteArray> transactions;
    for (int i = 0; i < 100; ++i)
    {
        QByteArray data;
        data.resize(i);
        memset(data.data(), i, data.size());
        transactions.push_back(data);
    }

    QByteArray serializedData = serializeTransactionList(transactions, 0);
    bool success = false;
    auto deserializedTransactions = deserializeTransactionList(serializedData, &success);
    QByteArray serializedData2 = serializeTransactionList(deserializedTransactions, 0);
    ASSERT_TRUE(success);
    ASSERT_EQ(serializedData.toHex(), serializedData2.toHex());
}

} // namespace test
} // namespace p2p
} // namespace nx
