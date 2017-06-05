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
        FAIL();
    }
}

TEST(P2pSerialization, CompressedPeers)
{
    QVector<PeerNumberType> peers;
    for (int i = 0; i < kMaxOnlineDistance; ++i)
        peers.push_back(i);

    QByteArray serializedData = serializeCompressedPeers(peers, 0);
    bool success = false;
    auto deserializedPeers = deserializeCompressedPeers(serializedData, &success);
    ASSERT_TRUE(success);
    ASSERT_EQ(peers, deserializedPeers);
}

TEST(P2pSerialization, CompressedSize)
{
    QVector<quint32> values;
    for (int i = 0; i < 1000000 * 500; i += 1000000)
        values.push_back(i);
    QVector<quint32> deserializedValues;

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
            deserializedValues.push_back(deserializeCompressedSize(reader));
    }
    catch (...)
    {
        FAIL();
    }
    ASSERT_EQ(values, deserializedValues);
}

TEST(P2pSerialization, PeersMessage)
{
    using namespace nx::p2p;

    QVector<PeerDistanceRecord> peers;
    for (int i = 0; i < 100; ++i)
        peers.push_back(PeerDistanceRecord(i, i * 1000));

    QByteArray expectedData = serializePeersMessage(peers, 0);
    bool success = false;
    auto deserializedPeers = deserializePeersMessage(expectedData, &success);
    QByteArray actualData = serializePeersMessage(deserializedPeers, 0);
    ASSERT_TRUE(success);
    ASSERT_EQ(expectedData.toHex(), actualData.toHex());
}

TEST(P2pSerialization, SubscribeRequest)
{
    using namespace nx::p2p;

    QVector<SubscribeRecord> peers;
    for (int i = 0; i < 100; ++i)
        peers.push_back(SubscribeRecord(i, i * 1000));

    QByteArray expectedData = serializeSubscribeRequest(peers, 0);
    bool success = false;
    auto deserializedPeers = deserializeSubscribeRequest(expectedData, &success);
    QByteArray actualData = serializeSubscribeRequest(deserializedPeers, 0);
    ASSERT_TRUE(success);
    ASSERT_EQ(expectedData.toHex(), actualData.toHex());
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

    QByteArray expectedData = serializeResolvePeerNumberResponse(peers, 0);
    bool success = false;
    auto deserializedPeers = deserializeResolvePeerNumberResponse(expectedData, &success);
    QByteArray actualData = serializeResolvePeerNumberResponse(deserializedPeers, 0);
    ASSERT_TRUE(success);
    ASSERT_EQ(expectedData.toHex(), actualData.toHex());
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

    QByteArray expectedData = serializeTransactionList(transactions, 0);
    bool success = false;
    auto deserializedTransactions = deserializeTransactionList(expectedData, &success);
    QByteArray actualData = serializeTransactionList(deserializedTransactions, 0);
    ASSERT_TRUE(success);
    ASSERT_EQ(expectedData.toHex(), actualData.toHex());
}

TEST(P2pSerialization, UnicastTransaction)
{
    using namespace nx::p2p;

    UnicastTransactionRecords records;
    for (int i = 0; i < 100; ++i)
        records.push_back(UnicastTransactionRecord(QnUuid::createUuid(), i));

    QByteArray expectedData = serializeUnicastHeader(records);
    int size = 0;
    auto deserializedRecords = deserializeUnicastHeader(expectedData, &size);
    ASSERT_TRUE(size > 0);
    QByteArray actualData = serializeUnicastHeader(deserializedRecords);
    ASSERT_EQ(expectedData.toHex(), actualData.toHex());

    deserializeUnicastHeader(QByteArray("1"), &size);
    ASSERT_EQ(-1, size); //< Deserialization error.
}

} // namespace test
} // namespace p2p
} // namespace nx
