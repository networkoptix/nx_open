// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>

#include <gtest/gtest.h>

#include <nx/codec/h264/sequence_parameter_set.h>
#include <nx/codec/h264/supplemental_enhancement_information.h>
#include <nx/codec/h265/hevc_common.h>
#include <nx/codec/h265/sequence_parameter_set.h>
#include <nx/codec/h265/supplemental_enhancement_information.h>
#include <nx/codec/nal_units.h>
#include <nx/codec/sei_common.h>

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
        {{"00000302 000003"}, {"000002 000003"}}};

    QByteArray source;
    QByteArray destination;
    for (const auto& entry: kPayloads)
    {
        source = QByteArray::fromHex(entry.first);
        const auto result = QByteArray::fromHex(entry.second);
        destination.resize(result.size());

        nx::media::nal::decodeEpb(
            (const quint8*) source.constData(), (const quint8*) source.constData() + source.size(),
            (quint8*) destination.data(), (size_t) destination.size());

        ASSERT_EQ(result, destination);
    }
}

TEST(NalUnits, h264SEIWrongPayloadSize)
{
    {
        const uint8_t data[] = {0x26, 0x05, 0x03, 0x34, 0xac, 0x56}; //< State size == 3.
        const auto sei = nx::media::h264::parseSeiUserData(std::span(data));
        ASSERT_TRUE(sei.has_value());
        ASSERT_EQ(sei->payloads.size(), 1);
        ASSERT_EQ(sei->payloads.front().size(), 3); //< Correct size.
    }
    {
        const uint8_t data[] = {0x26, 0x05, 0x04, 0x34, 0xac, 0x56}; //< State size == 4.
        const auto sei = nx::media::h264::parseSeiUserData(std::span(data));
        ASSERT_TRUE(sei.has_value());
        ASSERT_EQ(sei->payloads.size(), 1);
        ASSERT_EQ(sei->payloads.front().size(), 3); //< Truncated.
    }
    {
        const uint8_t data[] = {0x26, 0x05, 0xff, 0x01, 0x34, 0xac, 0x56}; //< State size == 256.
        const auto sei = nx::media::h264::parseSeiUserData(std::span(data));
        ASSERT_TRUE(sei.has_value());
        ASSERT_EQ(sei->payloads.size(), 1);
        ASSERT_EQ(sei->payloads.front().size(), 3); //< Truncated.
    }
    {
        // Each call owns its own decoded buffer, so results from separate calls stay independent.
        const uint8_t first[] = {0x26, 0x05, 0x02, 0x11, 0x22};
        const auto sei1 = nx::media::h264::parseSeiUserData(std::span(first));
        ASSERT_TRUE(sei1.has_value());
        ASSERT_EQ(sei1->payloads.size(), 1);

        const uint8_t second[] = {0x26, 0x05, 0x01, 0x99};
        const auto sei2 = nx::media::h264::parseSeiUserData(std::span(second));
        ASSERT_TRUE(sei2.has_value());
        ASSERT_EQ(sei2->payloads.size(), 1);
        ASSERT_EQ(sei2->payloads.front().front(), 0x99);

        // The first result is still valid and unaffected by the second call.
        ASSERT_EQ(sei1->payloads.front().size(), 2);
        ASSERT_EQ(sei1->payloads.front().front(), 0x11);
    }
}

// extractUserData() decodes the EBSP past the NAL header and parses it, common for h264 and h265.
TEST(NalUnits, seiExtractUserData)
{
    {
        // Two user_data_unregistered (type 5) messages in one NALU.
        const uint8_t data[] = {0x05, 0x03, 0xaa, 0xbb, 0xcc, //< Type 5, size 3.
                                0x05, 0x02, 0x11, 0x22, //< Type 5, size 2.
                                0x80}; //< rbsp_trailing_bits.
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 2);
        ASSERT_EQ(payloads.front().size(), 3);
        ASSERT_EQ(payloads.back().size(), 2);
    }
    {
        // Mix of message types; only type 5 is collected.
        const uint8_t data[] = {0x00, 0x01, 0x77, //< Type 0 (buffering_period), size 1.
                                0x05, 0x02, 0xde, 0xad, //< Type 5, size 2.
                                0x01, 0x02, 0x33, 0x44, //< Type 1 (pic_timing), size 2.
                                0x80};
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 1);
        const auto& payload = payloads.front();
        ASSERT_EQ(payload.size(), 2);
        ASSERT_EQ(memcmp(payload.data(), "\xde\xad", 2), 0);
    }
    {
        // 0xff-extended message type (255 + 6 == 261) is skipped, type 5 is collected.
        const uint8_t data[] = {0xff, 0x06, 0x02, 0xaa, 0xbb, //< Type 261, size 2.
                                0x05, 0x01, 0x99, //< Type 5, size 1.
                                0x80};
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 1);
        const auto& payload = payloads.front();
        ASSERT_EQ(payload.size(), 1);
        ASSERT_EQ(payload.front(), 0x99);
    }
    {
        // Type 5 with wrong size at the end of a mix: declared 5, only 2 bytes left.
        const uint8_t data[] = {0x00, 0x01, 0x00, //< Type 0, size 1.
                                0x05, 0x05, 0x12, 0x34}; //< Type 5, size 5, truncated.
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 1);
        const auto& payload = payloads.front();
        ASSERT_EQ(payload.size(), 2); //< Truncated.
        ASSERT_EQ(memcmp(payload.data(), "\x12\x34", 2), 0);
    }
    {
        // Empty type 5 payload.
        const uint8_t data[] = {
            0x05, 0x00, //< Type 5, size 0.
            0x80};
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 1);
        ASSERT_EQ(payloads.front().size(), 0);
    }
    {
        // Malformed: buffer ends before the message size byte.
        const uint8_t data[] = {0xff, 0x05};
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "Bad SEI detected. SEI too short - unable to read size");
    }
    {
        // Long 0xff-extended message type (4 * 255 + 0 == 1020) is skipped, type 5 is collected.
        const uint8_t data[] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0xaa, //< Type 1020, size 1.
                                0x05, 0x01, 0x99, //< Type 5, size 1.
                                0x80};
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 1);
        const auto& payload = payloads.front();
        ASSERT_EQ(payload.size(), 1);
        ASSERT_EQ(payload.front(), 0x99);
    }
    {
        // Long 0xff-extended message size (2 * 255 + 2 == 512) far beyond the buffer.
        const uint8_t data[] = {0x05, 0xff, 0xff,
                                0x02, 0x12, 0x34}; //< Type 5, size 512, truncated.
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 1);
        const auto& payload = payloads.front();
        ASSERT_EQ(payload.size(), 2); //< Truncated.
        ASSERT_EQ(memcmp(payload.data(), "\x12\x34", 2), 0);
    }
    {
        // Malformed: the message type 0xff run never terminates.
        const uint8_t data[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "Bad SEI detected. SEI too short - unable to read type");
    }
    {
        // Malformed: the message size 0xff run never terminates.
        const uint8_t data[] = {0x05, 0xff, 0xff, 0xff};
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "Bad SEI detected. SEI too short - unable to read size");
    }
    {
        // Regression: a 0xff-extended size run long enough to overflow a 32-bit accumulator
        // must not wrap to negative and walk the cursor backwards out of the buffer.
        constexpr int kFfRunLength = 8'422'000; //< 255 * kFfRunLength > INT_MAX.
        std::vector<uint8_t> data(1 + kFfRunLength + 1 + 2);
        data[0] = 0x05; //< Type 5.
        std::fill_n(data.begin() + 1, kFfRunLength, uint8_t(0xff));
        data[1 + kFfRunLength] = 0x02; //< Terminates the size run.
        data[2 + kFfRunLength] = 0xaa;
        data[3 + kFfRunLength] = 0xbb;
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 1);
        const auto& payload = payloads.front();
        ASSERT_EQ(payload.size(), 2); //< Declared size vastly exceeds the buffer; truncated to what remains.
        ASSERT_EQ(memcmp(payload.data(), "\xaa\xbb", 2), 0);
    }
    {
        // A single leftover trailing byte other than the rbsp_trailing_bits 0x80 (e.g. vendor
        // padding) does not fail the parsing of the preceding messages.
        const uint8_t data[] = {0x05, 0x02, 0x11, 0x22, //< Type 5, size 2.
                                0x00}; //< Leftover padding byte.
        const auto result = nx::media::sei::extractUserData(data);
        ASSERT_TRUE(result.has_value());
        const auto& payloads = result->payloads;
        ASSERT_EQ(payloads.size(), 1);
        ASSERT_EQ(payloads.front().size(), 2);
    }
}

TEST(NalUnits, h265SEIDeserialize)
{
    // Prefix SEI NALU: 2-byte NAL header, EPB in the payload (00 00 03 01 decodes to 00 00 01).
    const uint8_t data[] = {0x4e, 0x01, //< NAL header: prefixSeiNut (39).
                            0x05, 0x03, 0x00,
                            0x00, 0x03, 0x01, //< Type 5, size 3 (4 bytes with EPB).
                            0x80};
    const auto sei = nx::media::h265::parseSeiUserData(std::span(data));
    ASSERT_TRUE(sei.has_value());
    ASSERT_EQ(sei->payloads.size(), 1);
    const auto& payload = sei->payloads.front();
    ASSERT_EQ(payload.size(), 3);
    ASSERT_EQ(memcmp(payload.data(), "\x00\x00\x01", 3), 0);
}

TEST(NalUnits, h265SEIDeserializeIndependentCalls)
{
    // Each call owns its own decoded buffer, so results from separate calls stay independent.
    const uint8_t first[] = {0x4e, 0x01, 0x05, 0x02, 0x11, 0x22, 0x80};
    const auto sei1 = nx::media::h265::parseSeiUserData(std::span(first));
    ASSERT_TRUE(sei1.has_value());
    ASSERT_EQ(sei1->payloads.size(), 1);

    const uint8_t second[] = {0x4e, 0x01, 0x05, 0x01, 0x99, 0x80};
    const auto sei2 = nx::media::h265::parseSeiUserData(std::span(second));
    ASSERT_TRUE(sei2.has_value());
    ASSERT_EQ(sei2->payloads.size(), 1);
    ASSERT_EQ(sei2->payloads.front().front(), 0x99);

    // The first result is still valid and unaffected by the second call.
    ASSERT_EQ(sei1->payloads.front().size(), 2);
    ASSERT_EQ(sei1->payloads.front().front(), 0x11);
}

// Real vendor SEI NALUs captured from cameras. The declared payload sizes may exceed the
// captured NALU (stream padding is stripped together with the start codes), so some payloads
// come out truncated - which is exactly what the parser must survive.

static void checkVendorSei(
    auto parseSeiUserData, const QByteArray& nalu, const QByteArray& expectedUserData)
{
    const auto sei = parseSeiUserData(
        std::span((const uint8_t*) nalu.constData(), (size_t) nalu.size()));
    ASSERT_TRUE(sei.has_value());
    ASSERT_EQ(sei->payloads.size(), 1);
    const auto& payload = sei->payloads.front();
    ASSERT_EQ(payload.size(), expectedUserData.size());
    ASSERT_EQ(memcmp(payload.data(), expectedUserData.constData(), payload.size()), 0);
}

TEST(H26xVendorSei, h264AxisUserData)
{
    // Axis Q6086-E: 16-byte UUID, then [size][type] sub-messages (product info, timestamp).
    const QByteArray nalu = QByteArray::fromHex(
        "06052eaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa000d0a000a4effffffe82725234bb5000d0a016a"
        "4b18ac396a4b18ac390180");
    const QByteArray expected = QByteArray::fromHex(
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa000d0a000a4effffffe82725234bb5000d0a016a4b18ac"
        "396a4b18ac3901");
    checkVendorSei(&nx::media::h264::parseSeiUserData, nalu, expected);
}

TEST(H26xVendorSei, h265UniviewObjectTracking)
{
    // Uniview IPC2124SB: object tracking header (marker 0x20), no tracks. Heavy on EPB;
    // declared size 17, 13 bytes present.
    const QByteArray nalu = QByteArray::fromHex("4e01051120000003000003000003000100000300c880");
    const QByteArray expected = QByteArray::fromHex("200000000000000001000000c8");
    checkVendorSei(&nx::media::h265::parseSeiUserData, nalu, expected);
}

TEST(H26xVendorSei, h265UniviewPerFrameInfo)
{
    // Uniview IPC2124SB: per-frame info blob (marker 0x21); declared size 168, 121 bytes present.
    const QByteArray nalu = QByteArray::fromHex(
        "4e0105a8210000030000030000030001000101f400000300000300000300010000030000030000"
        "030001040c12ba15cc11b100000300000300000300000300000300000300000300000300000300"
        "000300000300000300000300000300000300000300000300000300000300000300000300000300"
        "000300000300000300000300000300000300000300020000030007000003000700000300000300"
        "0003000003000003000003000003000080");
    const QByteArray expected = QByteArray::fromHex(
        "210000000000000001000101f400000000000000010000000000000001040c12ba15cc11b10000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000020000000700000007000000000000000000000000"
        "00000000");
    checkVendorSei(&nx::media::h265::parseSeiUserData, nalu, expected);
}

TEST(H26xVendorSei, h265UniviewCameraInfo)
{
    // Uniview IPC2124SB: one-shot config blob (marker 0x10) with 0xff-extended message size
    // (0xff 0x3c == 315); 241 bytes present.
    const QByteArray nalu = QByteArray::fromHex(
        "4e0105ff3c100901010004808080800080800000030000e02e00001e0040066400000300c08302"
        "4a0c00000300050505000003000003030600000300000300000300000300000300000300000300"
        "0003000003000005036407000003003426370000030000030040030d0000600200640000030000"
        "030000030000030000030000030000030000030000030000030000030001000003005f5f030088"
        "13c20100000300000300002d2001004c3100000300000405040d14050004330b80802040000003"
        "000003000003000003000003000003000003000003000003000003000003000003000003000003"
        "000003000003000003000003000003000003000003000003000003000003000003000003000003"
        "000003000003000003000003000003000003000003000003000003000003000003000003000003"
        "000003000003000080");
    const QByteArray expected = QByteArray::fromHex(
        "1009010100048080808000808000000000e02e00001e00400664000000c083024a0c0000000505"
        "050000000003060000000000000000000000000000000000000000050364070000003426370000"
        "00000040030d000060020064000000000000000000000000000000000000000000000001000000"
        "5f5f03008813c2010000000000002d2001004c31000000000405040d14050004330b8080204000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000");
    checkVendorSei(&nx::media::h265::parseSeiUserData, nalu, expected);
}

TEST(NalUnits, h264SPSWrongResolution)
{
    nx::media::h264::SequenceParameterSet sps;
    uint8_t data[] = {0x27, 0xf4, 0x00, 0x34, 0xac, 0x56, 0x80, 0x50,
                      0x05, 0xba, 0x6e, 0x04, 0x04, 0x04, 0x04};
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
        {"0101", false}};

    for (const auto& entry: kEncodedSps)
    {
        const auto buffer = QByteArray::fromHex(entry.first);
        nx::media::h265::SequenceParameterSet sps;
        const auto result =
            nx::media::h265::parseNalUnit((const uint8_t*) buffer.constData(), buffer.size(), sps);
        ASSERT_EQ(result, entry.second);
    }
}

TEST(NalUnits, find4BytesStartcodes)
{
    // Simple
    {
        uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data));
        ASSERT_EQ(sizes.size(), 1);
        ASSERT_EQ(sizes[0].data, data + 4);
        ASSERT_EQ(sizes[0].size, 2);
    }
    {
        uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8, 0x00, 0x00,
                          0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data));
        ASSERT_EQ(sizes.size(), 2);
        ASSERT_EQ(sizes[0].data, data + 4);
        ASSERT_EQ(sizes[0].size, 2);
        ASSERT_EQ(sizes[1].data, data + 10);
        ASSERT_EQ(sizes[1].size, 5);
    }
    {
        uint8_t data[] = {0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1, 0x1c, 0xe8};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data));
        ASSERT_EQ(sizes.size(), 0);
    }

    // Trailing zeros
    {
        uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8, 0x00, 0x00, 0x00,
                          0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1, 0x00, 0x00};

        auto sizes = nx::media::nal::findNalUnitsAnnexB(data, sizeof(data));
        ASSERT_EQ(sizes.size(), 2);
        ASSERT_EQ(sizes[0].data, data + 4);
        ASSERT_EQ(sizes[0].size, 2);

        ASSERT_EQ(sizes[1].data, data + 11);
        ASSERT_EQ(sizes[1].size, 5);
    }

    // Dropped first startcode
    {
        uint8_t data[] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff,
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
        uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8};

        auto converted = nx::media::nal::convertStartCodesToSizes(data, sizeof(data));
        ASSERT_EQ(converted.size(), sizeof(data));
        ASSERT_EQ(converted[0], 0x00);
        ASSERT_EQ(converted[1], 0x00);
        ASSERT_EQ(converted[2], 0x00);
        ASSERT_EQ(converted[3], 0x02);
        ASSERT_EQ(converted[4], 0x1c);
        ASSERT_EQ(converted[5], 0xe8);

        nx::media::nal::convertToStartCodes(converted.data(), converted.size());
        ASSERT_EQ(0, memcmp(converted.data(), data, sizeof(data)));
    }
    {
        uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8, 0x00, 0x00,
                          0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1};

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

        nx::media::nal::convertToStartCodes(converted.data(), converted.size());
        ASSERT_EQ(0, memcmp(converted.data(), data, sizeof(data)));
    }
    // Padding.
    {
        uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8, 0x00, 0x00,
                          0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1};

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
        uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x1c, 0xe8, 0x00, 0x00, 0x00,
                          0x00, 0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1, 0x00, 0x00};

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
    // 3 bytes startcode
    {
        uint8_t data[] = {0x00, 0x00, 0x01, 0x1c, 0xe8, 0x00, 0x00,
                          0x01, 0xe5, 0xaf, 0xfc, 0x69, 0xb1};

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

TEST(NalUnits, convertToStartCodes)
{
    // These tests may not crash the process, so it's best to test them with Address Sanitizer or
    // Valgrind.

    // Broken data
    {
        uint8_t data[] = {0xff, 0xff, 0xff, 0xff, 0xe8};
        nx::media::nal::convertToStartCodes(data, sizeof(data));
    }

    // Broken data
    {
        uint8_t data[] = {0xff, 0xff, 0xf0, 0x00, 0xe8};
        nx::media::nal::convertToStartCodes(data, sizeof(data));
    }

    // Broken data
    {
        uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0xe8};
        nx::media::nal::convertToStartCodes(data, sizeof(data));
    }
}
