// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/codec/hevc/hevc_common.h>
#include <nx/codec/hevc/sequence_parameter_set.h>
#include <nx/codec/nal_units.h>

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

    QByteArray source;
    QByteArray destination;
    for (const auto& entry: kPayloads)
    {
        source = QByteArray::fromHex(entry.first);
        const auto result = QByteArray::fromHex(entry.second);
        destination.resize(result.size());

        NALUnit::decodeNAL(
            (const quint8*)source.constData(),
            (const quint8*)source.constData() + source.size(),
            (quint8*)destination.data(),
            (size_t)destination.size());

        ASSERT_EQ(result, destination);
    }
}

TEST(NalUnits, h264SEIWrongPayloadSize)
{
    SPSUnit sps;
    {
        SEIUnit sei;
        uint8_t data[] =
            {0x26, 0x00, 0x03, 0x34, 0xac, 0x56};
        sei.decodeBuffer(data, data + sizeof(data));
        ASSERT_TRUE(sei.deserialize(sps, 0));
    }
    {
        SEIUnit sei;
        uint8_t data[] =
            {0x26, 0x00, 0x04, 0x34, 0xac, 0x56};
        sei.decodeBuffer(data, data + sizeof(data));
        ASSERT_FALSE(sei.deserialize(sps, 0));
    }
    {
        SEIUnit sei;
        uint8_t data[] =
            {0x26, 0x00, 0xFF, 0x01, 0x34, 0xac, 0x56};
        sei.decodeBuffer(data, data + sizeof(data));
        ASSERT_FALSE(sei.deserialize(sps, 0));
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
        const auto buffer = QByteArray::fromHex(entry.first);
        nx::media::hevc::SequenceParameterSet sps;
        const auto result = nx::media::hevc::parseNalUnit(
            (const uint8_t*) buffer.constData(), buffer.size(), sps);
        ASSERT_EQ(result, entry.second);
    }
}

TEST(NalUnits, find4BytesStartcodes)
{
    // Simple
    {
        uint8_t data[] = {
            0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data));
        ASSERT_EQ(sizes.size(), 1);
        ASSERT_EQ(sizes[0].data, data + 4);
        ASSERT_EQ(sizes[0].size, 2);
    }
    {
        uint8_t data[] = {
            0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8,
            0x00, 0x00, 0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data));
        ASSERT_EQ(sizes.size(), 2);
        ASSERT_EQ(sizes[0].data, data + 4);
        ASSERT_EQ(sizes[0].size, 2);
        ASSERT_EQ(sizes[1].data, data + 10);
        ASSERT_EQ(sizes[1].size, 5);
    }
    {
        uint8_t data[] = {
            0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1, 0x1c, 0xe8};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data));
        ASSERT_EQ(sizes.size(), 0);
    }

    // Trailing zeros
    {
        uint8_t data[] = {
            0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8, 0x00,
            0x00, 0x00, 0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1, 0x00, 0x00};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data));
        ASSERT_EQ(sizes.size(), 2);
        ASSERT_EQ(sizes[0].data, data + 4);
        ASSERT_EQ(sizes[0].size, 2);

        ASSERT_EQ(sizes[1].data, data + 11);
        ASSERT_EQ(sizes[1].size, 5);
    }

    // Dropped first startcode
    {
        uint8_t data[] = {
            0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff,
            0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data), true);
        ASSERT_EQ(sizes.size(), 3);
        ASSERT_EQ(sizes[0].data, data);
        ASSERT_EQ(sizes[0].size, 4);

        ASSERT_EQ(sizes[1].data, data + 7);
        ASSERT_EQ(sizes[1].size, 3);

        ASSERT_EQ(sizes[2].data, data + 14);
        ASSERT_EQ(sizes[2].size, 6);
    }
}


TEST(NalUnits, convertStartCodesToSizes)
{
    // Simple.
    {
        uint8_t data[] = {
            0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8};

        auto converted = nx::media::nal::convertStartCodesToSizes(data, sizeof(data));
        ASSERT_EQ(converted.size(), sizeof(data));
        ASSERT_EQ(converted[0], 0x00);
        ASSERT_EQ(converted[1], 0x00);
        ASSERT_EQ(converted[2], 0x00);
        ASSERT_EQ(converted[3], 0x02);
        ASSERT_EQ(converted[4], 0x1c);
        ASSERT_EQ(converted[5], 0xe8);
    }
    {
        uint8_t data[] = {
            0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8,
            0x00, 0x00, 0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1};

        auto converted = nx::media::nal::convertStartCodesToSizes(data, sizeof(data));
        ASSERT_EQ(converted.size(), sizeof(data));
        ASSERT_EQ(converted[0], 0x00);
        ASSERT_EQ(converted[1], 0x00);
        ASSERT_EQ(converted[2], 0x00);
        ASSERT_EQ(converted[3], 0x02);
        ASSERT_EQ(converted[4], 0x1c);
        ASSERT_EQ(converted[5], 0xe8);

        ASSERT_EQ(converted[6], 0x00);
        ASSERT_EQ(converted[7], 0x00);
        ASSERT_EQ(converted[8], 0x00);
        ASSERT_EQ(converted[9], 0x05);
    }
    // Padding.
    {
        uint8_t data[] = {
            0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8,
            0x00, 0x00, 0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1};

        auto converted = nx::media::nal::convertStartCodesToSizes(data, sizeof(data), 32);
        ASSERT_EQ(converted.size(), sizeof(data) + 32);
        ASSERT_EQ(converted[0], 0x00);
        ASSERT_EQ(converted[1], 0x00);
        ASSERT_EQ(converted[2], 0x00);
        ASSERT_EQ(converted[3], 0x02);
        ASSERT_EQ(converted[4], 0x1c);
        ASSERT_EQ(converted[5], 0xe8);

        ASSERT_EQ(converted[6], 0x00);
        ASSERT_EQ(converted[7], 0x00);
        ASSERT_EQ(converted[8], 0x00);
        ASSERT_EQ(converted[9], 0x05);

        // Padding.
        ASSERT_EQ(converted[15], 0x00);
        ASSERT_EQ(converted[16], 0x00);
        ASSERT_EQ(converted[17], 0x00);
        ASSERT_EQ(converted[18], 0x00);
        ASSERT_EQ(converted[19], 0x00);

        ASSERT_EQ(converted[46], 0x00);
    }
    // Trailing zeros.
    {
        uint8_t data[] = {
            0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8, 0x00,
            0x00, 0x00, 0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1, 0x00, 0x00};

        auto converted = nx::media::nal::convertStartCodesToSizes(data, sizeof(data));
        ASSERT_EQ(converted.size(), sizeof(data) - 3);
        ASSERT_EQ(converted[0], 0x00);
        ASSERT_EQ(converted[1], 0x00);
        ASSERT_EQ(converted[2], 0x00);
        ASSERT_EQ(converted[3], 0x02);
        ASSERT_EQ(converted[4], 0x1c);
        ASSERT_EQ(converted[5], 0xe8);

        ASSERT_EQ(converted[6], 0x00);
        ASSERT_EQ(converted[7], 0x00);
        ASSERT_EQ(converted[8], 0x00);
        ASSERT_EQ(converted[9], 0x05);
    }
    // 3 bytes starcode
    {
        uint8_t data[] = {
            0x00, 0x00, 0x01, 0x1c, 0xe8,
            0x00, 0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1};

        auto converted = nx::media::nal::convertStartCodesToSizes(data, sizeof(data));
        ASSERT_EQ(converted.size(), sizeof(data) + 2);
        ASSERT_EQ(converted[0], 0x00);
        ASSERT_EQ(converted[1], 0x00);
        ASSERT_EQ(converted[2], 0x00);
        ASSERT_EQ(converted[3], 0x02);
        ASSERT_EQ(converted[4], 0x1c);
        ASSERT_EQ(converted[5], 0xe8);

        ASSERT_EQ(converted[6], 0x00);
        ASSERT_EQ(converted[7], 0x00);
        ASSERT_EQ(converted[8], 0x00);
        ASSERT_EQ(converted[9], 0x05);
    }
}
