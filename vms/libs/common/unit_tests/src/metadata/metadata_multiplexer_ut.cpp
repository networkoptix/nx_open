#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <motion/metadata_multiplexer.h>

namespace test {

namespace {

class MetadataReaderStub:
    public QnAbstractMotionArchiveConnection
{
public:
    MetadataReaderStub(int packetsToGenerate):
        m_packetsToGenerate(packetsToGenerate)
    {
    }

    virtual QnAbstractCompressedMetadataPtr getMotionData(qint64 timeUsec) override
    {
        if (m_packetsGenerated.size() >= m_packetsToGenerate)
            return nullptr;

        const auto newTimestamp = m_lastPacketTimestamp + 1;
        if (timeUsec < newTimestamp)
            return nullptr;

        m_lastPacketTimestamp = newTimestamp;
        auto packet = std::make_shared<QnCompressedMetadata>(MetadataType::Motion);
        m_packetsGenerated.push_back(packet);
        packet->timestamp = m_lastPacketTimestamp;
        return packet;
    }

    std::vector<QnAbstractCompressedMetadataPtr> packetsGenerated() const
    {
        return m_packetsGenerated;
    }

private:
    std::vector<QnAbstractCompressedMetadataPtr> m_packetsGenerated;
    qint64 m_lastPacketTimestamp = 0;
    const int m_packetsToGenerate;
};

//struct PacketComparator
//{
//    bool operator()(
//        const QnAbstractCompressedMetadataPtr& left,
//        const QnAbstractCompressedMetadataPtr& right)
//    {
//        return left->timestamp < right->timestamp;
//    }
//};

} // namespace

class MetadataMultiplexer:
    public ::testing::Test
{
protected:
    void givenRandomReaders()
    {
        for (int i = 0; i < 7; ++i)
        {
            m_readers.push_back(std::make_shared<MetadataReaderStub>(
                nx::utils::random::number<int>(10, 100)));
            m_metadataMultiplexer.add(m_readers.back());
        }
    }

    void addEmptyReader()
    {
        m_metadataMultiplexer.add(std::make_shared<MetadataReaderStub>(0));
    }

    void whenReadAllPackets()
    {
        for (;;)
        {
            auto packet = m_metadataMultiplexer.getMotionData(
                std::numeric_limits<qint64>::max());
            if (!packet)
                break;
            m_packetsRead.push_back(packet);
        }
    }

    void thenAllPacketsHaveBeenProvided()
    {
        fetchPacketsGenerated();

        auto packetsRead = m_packetsRead;
        std::sort(packetsRead.begin(), packetsRead.end());

        ASSERT_EQ(m_packetsGenerated, packetsRead);
    }

    void andPacketsReadAreSortedByTimestamp()
    {
        qint64 prevTimestamp = 0;
        for (const auto& packet: m_packetsRead)
        {
            ASSERT_GE(packet->timestamp, prevTimestamp);
            prevTimestamp = packet->timestamp;
        }
    }

private:
    ::MetadataMultiplexer m_metadataMultiplexer;
    std::vector<std::shared_ptr<MetadataReaderStub>> m_readers;
    std::vector<QnAbstractCompressedMetadataPtr> m_packetsRead;
    std::vector<QnAbstractCompressedMetadataPtr> m_packetsGenerated;

    void fetchPacketsGenerated()
    {
        for (const auto& reader: m_readers)
        {
            auto packetsGenerated = reader->packetsGenerated();
            std::move(
                packetsGenerated.begin(),
                packetsGenerated.end(),
                std::back_inserter(m_packetsGenerated));
        }

        std::sort(m_packetsGenerated.begin(), m_packetsGenerated.end());
    }
};

TEST_F(MetadataMultiplexer, provides_packets_of_multiple_sources_sorted_by_timestamp)
{
    givenRandomReaders();

    whenReadAllPackets();

    thenAllPacketsHaveBeenProvided();
    andPacketsReadAreSortedByTimestamp();
}

TEST_F(MetadataMultiplexer, dataless_reader_does_not_block_other_readers)
{
    givenRandomReaders();
    addEmptyReader();

    whenReadAllPackets();

    thenAllPacketsHaveBeenProvided();
    andPacketsReadAreSortedByTimestamp();
}

} // namespace test
