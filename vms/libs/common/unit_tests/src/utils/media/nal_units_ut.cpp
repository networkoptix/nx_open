#include <gtest/gtest.h>

#include <nx/network/buffer.h>

#include <utils/media/nalUnits.h>
#include <utils/media/hevc_sps.h>

using SourcePayload = QByteArray;
using DestinationPayload = QByteArray;
using ParsingResult = bool;

TEST(NalUnits, decodeNal)
{
    static const std::map<SourcePayload, DestinationPayload> kPayloads = {
        {{"00000300"}, {"000000"}},
        {{"00000301"}, {"000001"}},
        {{"00000302"}, {"000002"}},
        {{"00000303"}, {"000003"}},
        {{"00000304"}, {"00000304"}},
        {{"00000301 00000302"}, {"000001 000002"}},
        {{"00000302 00000302"}, {"000002 000002"}},
        {{"00000302 000003" }, {"000002 000003"}}
    };

    nx::Buffer source;
    nx::Buffer destination;
    for (const auto& entry: kPayloads)
    {
        source = nx::Buffer::fromHex(entry.first);
        const auto result = nx::Buffer::fromHex(entry.second);
        destination.resize(result.size());

        NALUnit::decodeNAL(
            (const quint8*)source.constData(),
            (const quint8*)source.constData() + source.size(),
            (quint8*)destination.data(),
            (size_t)destination.size());

        ASSERT_EQ(result, destination);
    }
}

TEST(NalUnits, h264SPSWrongResolution)
{
    SPSUnit sps;
    uint8_t data[] =
        {0x27, 0xf4, 0x00, 0x34, 0xac, 0x56, 0x80, 0x50, 0x05, 0xba, 0x6e, 0x04, 0x04, 0x04, 0x04};
    sps.decodeBuffer(data, data + sizeof(data));
    ASSERT_EQ(sps.deserialize(), 0);
    ASSERT_EQ(sps.getWidth(), 1280);
    ASSERT_EQ(sps.getHeight(), 720);
}

TEST(NalUnits, hevcDecodeSpsNoCrash)
{
    static const std::map<SourcePayload, ParsingResult> kEncodedSps = {
        {"", false},
        {"01", false},
        {"0101", false}
    };

    for (const auto& entry : kEncodedSps)
    {
        nx::media_utils::hevc::Sps sps;
        const auto buffer = nx::Buffer::fromHex(entry.first);
        const auto result = sps.decode((const uint8_t*) buffer.constData(), buffer.size());

        ASSERT_EQ(result, entry.second);
    }
}
