// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/codec/av1/av1_common.h>
#include <nx/codec/av1/sequence_header.h>

using namespace nx::media::av1;

namespace {

void assertLeb128RoundTrip(uint64_t value, int expectedSize)
{
    uint8_t buffer[kMaxLeb128Bytes];
    ASSERT_EQ(expectedSize, writeLeb128(value, buffer));

    uint64_t decoded = 0;
    ASSERT_EQ(expectedSize, readLeb128(buffer, expectedSize, &decoded));
    ASSERT_EQ(value, decoded);
}

} // namespace

TEST(Av1, Leb128RoundTrip)
{
    assertLeb128RoundTrip(0, 1);
    assertLeb128RoundTrip(1, 1);
    assertLeb128RoundTrip(127, 1);
    assertLeb128RoundTrip(128, 2);
    assertLeb128RoundTrip(16383, 2);
    assertLeb128RoundTrip(16384, 3);
    assertLeb128RoundTrip((1 << 21) - 1, 3);
    assertLeb128RoundTrip(1 << 21, 4);
    assertLeb128RoundTrip((1 << 28) - 1, 4);
    assertLeb128RoundTrip(1 << 28, 5);
    assertLeb128RoundTrip(0xffffffff, 5);
}

TEST(Av1, Leb128ReadsNonMinimalEncoding)
{
    const uint8_t data[] = {0x80, 0x80, 0x00};
    uint64_t value = 42;
    ASSERT_EQ(3, readLeb128(data, sizeof(data), &value));
    ASSERT_EQ(0u, value);
}

TEST(Av1, Leb128RejectsTruncatedData)
{
    const uint8_t data[] = {0x80, 0x80};
    uint64_t value = 0;
    ASSERT_EQ(0, readLeb128(data, sizeof(data), &value));
    ASSERT_EQ(0, readLeb128(data, 0, &value));
}

TEST(Av1, Leb128RejectsTooLongEncoding)
{
    // 8 bytes with the continuation bit set in all of them.
    const uint8_t data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    uint64_t value = 0;
    ASSERT_EQ(0, readLeb128(data, sizeof(data), &value));
}

TEST(Av1, Leb128RejectsValueAbove32Bits)
{
    // 0x100000000, one more than the maximum allowed by the spec.
    const uint8_t data[] = {0x80, 0x80, 0x80, 0x80, 0x10};
    uint64_t value = 0;
    ASSERT_EQ(0, readLeb128(data, sizeof(data), &value));
}

TEST(Av1, ObuHeaderDecodesSingleByte)
{
    // obu_type = 1 (sequence header), obu_has_size_field = 1.
    const uint8_t data[] = {0x0a};
    ObuHeader header;
    ASSERT_EQ(1, header.decode(data, sizeof(data)));
    ASSERT_EQ(ObuType::sequenceHeader, header.type);
    ASSERT_FALSE(header.hasExtension);
    ASSERT_TRUE(header.hasSizeField);
}

TEST(Av1, ObuHeaderDecodesExtension)
{
    // obu_type = 6 (frame), obu_extension_flag = 1, obu_has_size_field = 1.
    // Extension: temporal_id = 3, spatial_id = 2, reserved = 5.
    const uint8_t data[] = {0x36, 0x75};
    ObuHeader header;
    ASSERT_EQ(2, header.decode(data, sizeof(data)));
    ASSERT_EQ(ObuType::frame, header.type);
    ASSERT_TRUE(header.hasExtension);
    ASSERT_TRUE(header.hasSizeField);
    ASSERT_EQ(3, header.temporalId);
    ASSERT_EQ(2, header.spatialId);
    ASSERT_EQ(5, header.extensionReserved);
}

TEST(Av1, ObuHeaderRejectsForbiddenBit)
{
    const uint8_t data[] = {0x8a};
    ObuHeader header;
    ASSERT_EQ(0, header.decode(data, sizeof(data)));
}

TEST(Av1, ObuHeaderRejectsTruncatedExtension)
{
    const uint8_t data[] = {0x36};
    ObuHeader header;
    ASSERT_EQ(0, header.decode(data, sizeof(data)));
}

TEST(Av1, ObuHeaderEncodeAddsSizeField)
{
    // obu_type = 6 (frame), no size field.
    const uint8_t data[] = {0x30};
    ObuHeader header;
    ASSERT_EQ(1, header.decode(data, sizeof(data)));
    ASSERT_FALSE(header.hasSizeField);

    uint8_t buffer[ObuHeader::kMaxSize] = {};
    ASSERT_EQ(1, header.encode(buffer, /*withSizeField*/ true));
    ASSERT_EQ(0x32, buffer[0]);
}

TEST(Av1, ObuHeaderEncodeRoundTrip)
{
    const uint8_t data[] = {0x37, 0x75}; //< Extension, size field and the reserved bit are set.
    ObuHeader header;
    ASSERT_EQ(2, header.decode(data, sizeof(data)));

    uint8_t buffer[ObuHeader::kMaxSize] = {};
    ASSERT_EQ(2, header.encode(buffer, /*withSizeField*/ true));
    ASSERT_EQ(data[0], buffer[0]);
    ASSERT_EQ(data[1], buffer[1]);
}

TEST(Av1, TemporalDelimiterObuIsValid)
{
    ObuHeader header;
    ASSERT_EQ(1, header.decode(kTemporalDelimiterObu, sizeof(kTemporalDelimiterObu)));
    ASSERT_EQ(ObuType::temporalDelimiter, header.type);
    ASSERT_TRUE(header.hasSizeField);

    uint64_t size = 1;
    ASSERT_EQ(1, readLeb128(kTemporalDelimiterObu + 1, 1, &size));
    ASSERT_EQ(0u, size);
}

namespace {

// clang-format off

// Sequence header OBU payloads (without obu_header and obu_size) produced by libaom-av1.
const uint8_t kSequenceHeader1920x1080[] = {
    0x20, 0x00, 0x00, 0x42, 0xab, 0xbf, 0xc3, 0x73, 0x2b, 0xe4, 0x80, 0x86, 0x80, 0x10};

const uint8_t kSequenceHeader1280x720[] = {
    0x20, 0x00, 0x00, 0x2d, 0x4c, 0xff, 0xb3, 0xcc, 0xaf, 0x92, 0x02, 0x1a, 0x00, 0x40};

const uint8_t kSequenceHeader640x360[] = {
    0x20, 0x00, 0x00, 0x0c, 0xc4, 0xff, 0x67, 0x36, 0xbe, 0x48, 0x08, 0x68, 0x01};

// Synthetic: reduced_still_picture_header = 1, profile 0, level 5, 1920x1080.
const uint8_t kSequenceHeaderStillPicture[] = {
    0x19, 0x7f, 0xc1, 0xdf, 0xc1, 0x0d, 0xc0};

// Synthetic: profile 2, timing_info() with equal_picture_interval, decoder_model_info(),
// 3 operating points with operating_parameters_info() and initial_display_delay, 3840x2160.
const uint8_t kSequenceHeaderTimingAndDecoderModel[] = {
    0x44, 0x00, 0x3d, 0x09, 0x00, 0x00, 0x01, 0x77, 0x02, 0x00, 0xfa, 0x69, 0x00, 0x00, 0x03,
    0x09, 0x21, 0xa2, 0x10, 0x04, 0xe0, 0x28, 0x0f, 0x90, 0x80, 0x92, 0x02, 0x80, 0xf9, 0x08,
    0x11, 0x20, 0x28, 0x0f, 0x95, 0xdf, 0x7f, 0xc3, 0x7c};

// clang-format on

} // namespace

TEST(Av1, SequenceHeaderParses1080p)
{
    SequenceHeader header;
    ASSERT_TRUE(header.read(kSequenceHeader1920x1080, sizeof(kSequenceHeader1920x1080)));
    ASSERT_EQ(1920, header.maxFrameWidth);
    ASSERT_EQ(1080, header.maxFrameHeight);
    ASSERT_EQ(1, header.seqProfile);
    ASSERT_FALSE(header.stillPicture);
    ASSERT_FALSE(header.reducedStillPictureHeader);
    // seq_level_idx[0] > 7, so seq_tier[0] is present in the bitstream.
    ASSERT_EQ(8, header.seqLevelIdx0);
    ASSERT_EQ(0, header.seqTier0);
    ASSERT_FALSE(header.frameIdNumbersPresentFlag);
}

TEST(Av1, SequenceHeaderParses720p)
{
    SequenceHeader header;
    ASSERT_TRUE(header.read(kSequenceHeader1280x720, sizeof(kSequenceHeader1280x720)));
    ASSERT_EQ(1280, header.maxFrameWidth);
    ASSERT_EQ(720, header.maxFrameHeight);
    ASSERT_EQ(5, header.seqLevelIdx0); //< No seq_tier[0] in the bitstream.
    ASSERT_EQ(0, header.seqTier0);
}

TEST(Av1, SequenceHeaderParses360p)
{
    SequenceHeader header;
    ASSERT_TRUE(header.read(kSequenceHeader640x360, sizeof(kSequenceHeader640x360)));
    ASSERT_EQ(640, header.maxFrameWidth);
    ASSERT_EQ(360, header.maxFrameHeight);
}

TEST(Av1, SequenceHeaderParsesReducedStillPicture)
{
    SequenceHeader header;
    ASSERT_TRUE(header.read(kSequenceHeaderStillPicture, sizeof(kSequenceHeaderStillPicture)));
    ASSERT_TRUE(header.stillPicture);
    ASSERT_TRUE(header.reducedStillPictureHeader);
    ASSERT_EQ(0, header.seqProfile);
    ASSERT_EQ(5, header.seqLevelIdx0);
    ASSERT_EQ(1920, header.maxFrameWidth);
    ASSERT_EQ(1080, header.maxFrameHeight);
    ASSERT_FALSE(header.frameIdNumbersPresentFlag);
}

TEST(Av1, SequenceHeaderParsesTimingInfoAndDecoderModel)
{
    SequenceHeader header;
    ASSERT_TRUE(header.read(
        kSequenceHeaderTimingAndDecoderModel, sizeof(kSequenceHeaderTimingAndDecoderModel)));
    ASSERT_EQ(2, header.seqProfile);
    ASSERT_EQ(9, header.seqLevelIdx0);
    ASSERT_EQ(1, header.seqTier0);
    ASSERT_EQ(3840, header.maxFrameWidth);
    ASSERT_EQ(2160, header.maxFrameHeight);
    ASSERT_TRUE(header.frameIdNumbersPresentFlag);
}

TEST(Av1, SequenceHeaderRejectsUnterminatedUvlc)
{
    // timing_info_present_flag and equal_picture_interval are set, and the uvlc() that follows has
    // no terminating one bit before the data ends. Reading a uvlc() consumes that bit however many
    // leading zeros there are, so the loop has to be stopped by the end of the data.
    // clang-format off
    const uint8_t data[] = {
        0x04, //< seq_profile = 0, still_picture = 0, reduced_still_picture_header = 0,
              //< timing_info_present_flag = 1, then num_units_in_display_tick starts.
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //< The rest of the 32 + 32 bits of timing_info().
        0x02, //< equal_picture_interval = 1, then the uvlc() starts with a zero bit.
        0x00, 0x00, 0x00, 0x00, 0x00}; //< 40 more leading zeros and no terminating one bit.
    // clang-format on

    SequenceHeader header;
    ASSERT_FALSE(header.read(data, sizeof(data)));
}

TEST(Av1, SequenceHeaderRejectsTruncatedData)
{
    // The parsed part of this sequence header ends 61 bits in, so anything shorter than 8 bytes
    // must be rejected, and must be rejected without reading out of bounds.
    constexpr int kParsedSize = 8;
    for (int size = 0; size < kParsedSize; ++size)
    {
        SequenceHeader header;
        ASSERT_FALSE(header.read(kSequenceHeader1920x1080, size)) << "size: " << size;
    }
}

TEST(Av1, SequenceHeaderDoesNotNeedTheTrailingPart)
{
    // Only the beginning of the sequence header is parsed: the frame size must be available
    // without color_config() and the rest of the header being present.
    SequenceHeader header;
    ASSERT_TRUE(header.read(kSequenceHeader1920x1080, /*size*/ 8));
    ASSERT_EQ(1920, header.maxFrameWidth);
    ASSERT_EQ(1080, header.maxFrameHeight);
}
